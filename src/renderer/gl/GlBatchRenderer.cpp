//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "GlBatchRenderer.h"
#include "tier0/dbg.h"
#include "mathsfury/Maths.h"
#include <glad/gl.h>
#include <string.h>
#include <pthread.h>

#ifdef SIMD_ENABLED
#include "simde/x86/sse2.h"

static void TransformVerticesSSE(
	char* pDst,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
	const simde__m128 col0 = simde_mm_loadu_ps(&matrix.m_Data[0]);
	const simde__m128 col1 = simde_mm_loadu_ps(&matrix.m_Data[4]);
	const simde__m128 col2 = simde_mm_loadu_ps(&matrix.m_Data[8]);
	const simde__m128 col3 = simde_mm_loadu_ps(&matrix.m_Data[12]);

	for (uint32 i = 0; i < vertexCount; i++)
	{
		float32* pPos = reinterpret_cast<float32*>(
			pDst + uint64(i) * vertexStride + vertexPosOffset);

		const simde__m128 result = simde_mm_add_ps(
			simde_mm_add_ps(
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[0]), col0),
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[1]), col1)),
			simde_mm_add_ps(
				simde_mm_mul_ps(simde_mm_set1_ps(pPos[2]), col2),
				col3));

		float32 tmp[4];
		simde_mm_storeu_ps(tmp, result);
		pPos[0] = tmp[0];
		pPos[1] = tmp[1];
		pPos[2] = tmp[2];
	}
}
#endif // SIMD_ENABLED

static void TransformVertices(
	char* pDst,
	uint32 vertexCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	const CMatrix4& matrix)
{
#ifdef SIMD_ENABLED
	TransformVerticesSSE(
		pDst,
		vertexCount,
		vertexStride,
		vertexPosOffset,
		matrix);
#else // SIMD_ENABLED
	CBatchRenderer::TransformVertices(
		pDst,
		vertexCount,
		vertexStride,
		vertexPosOffset,
		matrix);
#endif // !SIMD_ENABLED
}

static void* BatchTransforms(void* pArg)
{
	BatchThreadData_t* pBatchThreadData = static_cast<BatchThreadData_t*>(pArg);
	for (uint32 i = pBatchThreadData->m_ThreadStart; i < pBatchThreadData->m_ThreadEnd; i++)
	{
		const uint32 dstIndex = i - pBatchThreadData->m_ChunkStart;
		char* pDstVertexData = pBatchThreadData->m_pDstVertexData + uint64(dstIndex) * pBatchThreadData->m_VertexCount * pBatchThreadData->m_VertexLayoutStride;
		memcpy(
			pDstVertexData,
			pBatchThreadData->m_pSrcVertexData,
			uint64(pBatchThreadData->m_VertexCount) *
			pBatchThreadData->m_VertexLayoutStride);

		if (pBatchThreadData->m_HasVertexPos)
		{
			TransformVertices(
				pDstVertexData,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				(*pBatchThreadData->m_pBatchChunkTransforms)[i].ToMatrix());
		}
	}

	return GCMGL_NULL;
}

#ifdef SIMD_ENABLED
static void* FrustumCullThreadSSE(void* pArg)
{
	CullThreadData_t* pData = static_cast<CullThreadData_t*>(pArg);

	for (uint32 instanceIndex = pData->m_Start; instanceIndex < pData->m_End; instanceIndex++)
	{
		const BatchChunkTransform_t& batchChunkTransform = pData->m_pSrcTransforms[instanceIndex];

		const simde__m128 center = simde_mm_set_ps(
			1.0f,
			batchChunkTransform.m_Position.m_Z,
			batchChunkTransform.m_Position.m_Y,
			batchChunkTransform.m_Position.m_X);
		const simde__m128 extent = simde_mm_set_ps(
			0.0f,
			batchChunkTransform.m_Scale.m_Z * 0.5f,
			batchChunkTransform.m_Scale.m_Y * 0.5f,
			batchChunkTransform.m_Scale.m_X * 0.5f);

		bool isVisible = true;
		for (int32 i = 0; i < 6; i++)
		{
			const simde__m128 plane = simde_mm_set_ps(
				pData->m_pPlanes[i].m_Distance,
				pData->m_pPlanes[i].m_Normal.m_Z,
				pData->m_pPlanes[i].m_Normal.m_Y,
				pData->m_pPlanes[i].m_Normal.m_X);
			const simde__m128 absPlane = simde_mm_andnot_ps(
				simde_mm_set1_ps(-0.0f), plane);
			const simde__m128 r4 = simde_mm_mul_ps(absPlane, extent);
			const simde__m128 d4 = simde_mm_mul_ps(plane, center);

			float32 r[4], d[4];
			simde_mm_storeu_ps(r, r4);
			simde_mm_storeu_ps(d, d4);

			const float32 radius = r[0] + r[1] + r[2];
			const float32 dist = d[0] + d[1] + d[2] + d[3];

			if (dist + radius < 0.0f)
			{
				isVisible = false;

				break;
			}
		}

		if (isVisible)
		{
			pData->m_pDstTransforms->AddToTail(batchChunkTransform);
		}
	}

	return GCMGL_NULL;
}
#endif // SIMD_ENABLED

static void* FrustumCullThread(void* pArg)
{
	CullThreadData_t* pData = static_cast<CullThreadData_t*>(pArg);

	CBatchRenderer::CullChunk(
		pData->m_pSrcTransforms,
		pData->m_Start,
		pData->m_End,
		pData->m_pPlanes,
		*pData->m_pDstTransforms);

	return GCMGL_NULL;
}

void CGlBatchRenderer::FrustumCullBatch(
	const CBatch& batch,
	const Plane_t* pFrustumPlanes,
	CUtlVector<BatchChunkTransform_t>& batchChunkTransforms)
{
	for (int32 chunkIndex = 0; chunkIndex < batch.m_BatchChunks.Count(); chunkIndex++)
	{
		const BatchChunk_t& batchChunk = batch.m_BatchChunks[chunkIndex];

		if (batch.m_pCameraPos)
		{
			static const float32 s_ChunkCullRadius = 20000.0f;
			static const float32 s_ChunkCullRadiusSq = s_ChunkCullRadius * s_ChunkCullRadius;
			const CVector3 chunkOffset = batchChunk.m_Center - *batch.m_pCameraPos;
			if (chunkOffset.LengthSq() > s_ChunkCullRadiusSq)
			{
				continue;
			}
		}

		const int32 batchChunkTransformCount = batchChunk.m_BatchChunkTransforms.Count();

#ifdef THREADING_ENABLED
		if (batchChunkTransformCount > 1)
		{
			static const uint32 numThreads = 2;
			CUtlVector<BatchChunkTransform_t> threadOutput[numThreads];
			CullThreadData_t cullThreadData[numThreads];
			const uint32 halfCount = uint32(batchChunkTransformCount) / 2;
			pthread_t threads[numThreads];

			for (uint32 j = 0; j < numThreads; j++)
			{
				cullThreadData[j].m_pSrcTransforms =
					batchChunk.m_BatchChunkTransforms.Base();
				cullThreadData[j].m_pDstTransforms = &threadOutput[j];
				cullThreadData[j].m_pPlanes = pFrustumPlanes;
				cullThreadData[j].m_Start = j == 0 ? 0 : halfCount;
				cullThreadData[j].m_End =
					j == 0 ? halfCount : uint32(batchChunkTransformCount);

				const int32 threadCreateResult = pthread_create(
					&threads[j],
					GCMGL_NULL,
#ifdef SIMD_ENABLED
					FrustumCullThreadSSE,
#else // SIMD_ENABLED
					FrustumCullThread,
#endif // !SIMD_ENABLED
					&cullThreadData[j]);
				if (threadCreateResult != 0)
				{
					Warning(
						"[GlBatchRenderer] Failed to create thread %d: %d\n",
						j,
						threadCreateResult);
				}
			}

			for (uint32 j = 0; j < numThreads; j++)
			{
				pthread_join(threads[j], GCMGL_NULL);
			}

			for (uint32 j = 0; j < numThreads; j++)
			{
				for (int32 resultIndex = 0;
					resultIndex < threadOutput[j].Count();
					resultIndex++)
				{
					batchChunkTransforms.AddToTail(
						threadOutput[j][resultIndex]);
				}
			}

			continue;
		}
#endif // THREADING_ENABLED

#ifdef SIMD_ENABLED
		for (int32 instanceIndex = 0; instanceIndex < batchChunkTransformCount; instanceIndex++)
		{
			const BatchChunkTransform_t& transform =
				batchChunk.m_BatchChunkTransforms[instanceIndex];

			const simde__m128 center = simde_mm_set_ps(
				1.0f,
				transform.m_Position.m_Z,
				transform.m_Position.m_Y,
				transform.m_Position.m_X);
			const simde__m128 extent = simde_mm_set_ps(
				0.0f,
				transform.m_Scale.m_Z * 0.5f,
				transform.m_Scale.m_Y * 0.5f,
				transform.m_Scale.m_X * 0.5f);

			bool isVisible = true;
			for (int32 i = 0; i < 6; i++)
			{
				const simde__m128 plane = simde_mm_set_ps(
					pFrustumPlanes[i].m_Distance,
					pFrustumPlanes[i].m_Normal.m_Z,
					pFrustumPlanes[i].m_Normal.m_Y,
					pFrustumPlanes[i].m_Normal.m_X);
				const simde__m128 absPlane = simde_mm_andnot_ps(
					simde_mm_set1_ps(-0.0f), plane);
				const simde__m128 r4 = simde_mm_mul_ps(absPlane, extent);
				const simde__m128 d4 = simde_mm_mul_ps(plane, center);

				float32 r[4], d[4];
				simde_mm_storeu_ps(r, r4);
				simde_mm_storeu_ps(d, d4);

				const float32 radius = r[0] + r[1] + r[2];
				const float32 dist = d[0] + d[1] + d[2] + d[3];

				if (dist + radius < 0.0f)
				{
					isVisible = false;

					break;
				}
			}

			if (isVisible)
			{
				batchChunkTransforms.AddToTail(transform);
			}
		}
#else // SIMD_ENABLED
		CBatchRenderer::CullChunk(
			batchChunk.m_BatchChunkTransforms.Base(),
			0,
			uint32(batchChunkTransformCount),
			pFrustumPlanes,
			batchChunkTransforms);
#endif // !SIMD_ENABLED
	}
}

void CGlBatchRenderer::DrawBatchedChunk(
	uint32 vertexCount,
	const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startVertex)
{
	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(
			m_BufferResources.Find(m_PipelineState.m_hVertexBuffer)).m_pPtr) + (uint64(startVertex) * pVertexLayout->GetStride());
	uint32 vertexStride = pVertexLayout->GetStride();
	uint32 totalVertices = vertexCount * chunkSize;
	uint32 totalVertexDataSize = totalVertices * vertexStride;
	int32 vertexBufferIndex = m_BufferResources.Find(
		m_PipelineState.m_hVertexBuffer);
	if (vertexBufferIndex == m_BufferResources.InvalidIndex()) return;

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (uint32(stagingVertexBuffer.m_Data.Count()) < totalVertexDataSize)
	{
		stagingVertexBuffer.m_Data.SetCount(int32(totalVertexDataSize));
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}

		const BufferResource_t stagingVertexBufferResource = {
			uint64(stagingVertexBuffer.m_Data.Count()),
			stagingVertexBuffer.m_Data.Base(),
			stagingVertexBuffer.m_hId,
			GL_ARRAY_BUFFER,
			false
		};
		m_BufferResources.Insert(
			stagingVertexBuffer.m_hBuffer,
			stagingVertexBufferResource);
	}

	char* pDstVertexData = reinterpret_cast<char*>(
		stagingVertexBuffer.m_Data.Base());

	const uint32 vertexPosOffset = m_VertexPosOffset;
	const bool hasVertexPos = m_HasVertexPos;

#ifdef THREADING_ENABLED
	const uint32 numThreads = CMaths::Min((chunkSize > 1) ? 2u : 1u, chunkSize);
	//if (numThreads == 0) numThread = 4; // fallback
	//numThread = 2; // match PS3
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		pthread_t threads[2];
		const uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			const uint32 threadStart = chunkStart + i * batchesPerThread;
			const uint32 threadEnd = CMaths::Min(
				threadStart + batchesPerThread,
				chunkStart + chunkSize);
			if (threadStart >= chunkStart + chunkSize) break;

			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pDstIndexData = GCMGL_NULL;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pSrcIndices = GCMGL_NULL;
			batchThreadData[i].m_pBatchChunkTransforms = &batchChunkTransforms;
			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_ChunkStart = chunkStart;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_IndexCount = 0;
			batchThreadData[i].m_VertexLayoutStride = vertexStride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;

			int32 threadCreateResult = pthread_create(
				&threads[i],
				GCMGL_NULL,
				BatchTransforms,
				&batchThreadData[i]);
			if (threadCreateResult != 0)
			{
				Warning(
					"[GlBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadCreateResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			pthread_join(threads[i], GCMGL_NULL);
		}
	}
#endif // THREADING_ENABLED
#ifndef THREADING_ENABLED
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pBatchVertexDst = pDstVertexData + uint64(i) * vertexCount * vertexStride;
			memcpy(
				pBatchVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);

			if (hasVertexPos)
			{
				TransformVertices(
					pBatchVertexDst,
					vertexCount,
					vertexStride,
					vertexPosOffset,
					batchChunkTransforms[chunkStart + i].ToMatrix());
			}
		}
	}
#endif // !THREADING_ENABLED

	if (stagingVertexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingVertexBuffer.m_hId);
	}

	glBindBuffer(GL_ARRAY_BUFFER, stagingVertexBuffer.m_hId);
	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalVertexDataSize),
		stagingVertexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	const BufferResource_t stagingVertexBufferResource = {
		uint64(totalVertexDataSize),
		stagingVertexBuffer.m_Data.Base(),
		stagingVertexBuffer.m_hId,
		GL_ARRAY_BUFFER,
		false
	};
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	Draw(totalVertices);

	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags | StateDirtyFlags_t::VertexBuffer;

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}

static void* BatchIndexedTransforms(void* pArg)
{
	BatchThreadData_t* pBatchThreadData = static_cast<BatchThreadData_t*>(pArg);
	for (uint32 i = pBatchThreadData->m_ThreadStart; i < pBatchThreadData->m_ThreadEnd; i++)
	{
		const uint32 dstIndex = i - pBatchThreadData->m_ChunkStart;
		char* pDstVertexData = pBatchThreadData->m_pDstVertexData + uint64(dstIndex) * pBatchThreadData->m_VertexCount * pBatchThreadData->m_VertexLayoutStride;
		uint32* pDstIndexData = pBatchThreadData->m_pDstIndexData + uint64(dstIndex) * pBatchThreadData->m_IndexCount;
		const uint32 vertexBase = dstIndex * pBatchThreadData->m_VertexCount;

		memcpy(
			pDstVertexData,
			pBatchThreadData->m_pSrcVertexData,
			uint64(pBatchThreadData->m_VertexCount) *
			pBatchThreadData->m_VertexLayoutStride);

		if (pBatchThreadData->m_HasVertexPos)
		{
			TransformVertices(
				pDstVertexData,
				pBatchThreadData->m_VertexCount,
				pBatchThreadData->m_VertexLayoutStride,
				pBatchThreadData->m_VertexPosOffset,
				(*pBatchThreadData->m_pBatchChunkTransforms)[i].ToMatrix());
		}

		for (uint32 j = 0; j < pBatchThreadData->m_IndexCount; j++)
		{
			pDstIndexData[j] = pBatchThreadData->m_pSrcIndices[j] + vertexBase;
		}
	}

	return GCMGL_NULL;
}

void CGlBatchRenderer::DrawIndexedBatchedChunk(
	uint32 indexCount,
	uint32 vertexCount,
	const CUtlVector<BatchChunkTransform_t>& batchChunkTransforms,
	uint32 chunkStart,
	uint32 chunkSize,
	uint32 startIndex,
	int32 baseVertex)
{
	const CVertexLayout* pVertexLayout = m_PipelineState.m_pVertexLayout;
	const char* pSrcVertexData = reinterpret_cast<const char*>(
		m_BufferResources.Element(
			m_BufferResources.Find(m_PipelineState.m_hVertexBuffer)).m_pPtr);
	const uint32* pSrcIndices = reinterpret_cast<const uint32*>(
		reinterpret_cast<const char*>(
			m_BufferResources.Element(
				m_BufferResources.Find(m_PipelineState.m_hIndexBuffer)).m_pPtr) +
		(uint64(startIndex) * sizeof(uint32)));
	uint32 vertexStride = pVertexLayout->GetStride();
	uint32 totalVertices = vertexCount * chunkSize;
	uint32 totalIndices = indexCount * chunkSize;
	const uint64 totalVertexDataSize = uint64(totalVertices) * vertexStride;
	const uint64 totalIndexDataSize = uint64(totalIndices) * sizeof(uint32);
	int32 vertexBufferIndex = m_BufferResources.Find(m_PipelineState.m_hVertexBuffer);
	int32 indexBufferIndex = m_BufferResources.Find(m_PipelineState.m_hIndexBuffer);

	if (vertexBufferIndex == m_BufferResources.InvalidIndex() || indexBufferIndex == m_BufferResources.InvalidIndex())
	{
		return;
	}

	StagingBuffer_t& stagingVertexBuffer = m_StagingVertexBuffer[m_StagingIndex];
	if (uint64(stagingVertexBuffer.m_Data.Count()) < totalVertexDataSize)
	{
		stagingVertexBuffer.m_Data.SetCount(int32(totalVertexDataSize));
		if (!stagingVertexBuffer.m_hBuffer)
		{
			stagingVertexBuffer.m_hBuffer = m_NextHandle++;
		}

		const BufferResource_t stagingVertexBufferResource = {
			uint64(stagingVertexBuffer.m_Data.Count()),
			stagingVertexBuffer.m_Data.Base(),
			stagingVertexBuffer.m_hId,
			GL_ARRAY_BUFFER,
			false
		};
		m_BufferResources.Insert(
			stagingVertexBuffer.m_hBuffer,
			stagingVertexBufferResource);
	}

	char* pDstVertexData = reinterpret_cast<char*>(
		stagingVertexBuffer.m_Data.Base());

	StagingBuffer_t& stagingIndexBuffer = m_StagingIndexBuffer[m_StagingIndex];
	if (uint64(stagingIndexBuffer.m_Data.Count()) < totalIndexDataSize)
	{
		stagingIndexBuffer.m_Data.SetCount(int32(totalIndexDataSize));
		if (!stagingIndexBuffer.m_hBuffer)
		{
			stagingIndexBuffer.m_hBuffer = m_NextHandle++;
		}

		const BufferResource_t stagingIndexBufferResource = {
			uint64(stagingIndexBuffer.m_Data.Count()),
			stagingIndexBuffer.m_Data.Base(),
			stagingIndexBuffer.m_hId,
			GL_ELEMENT_ARRAY_BUFFER,
			false
		};
		m_BufferResources.Insert(
			stagingIndexBuffer.m_hBuffer,
			stagingIndexBufferResource);
	}

	uint32* pDstIndexData = reinterpret_cast<uint32*>(
		stagingIndexBuffer.m_Data.Base());

	const uint32 vertexPosOffset = m_VertexPosOffset;
	const bool hasVertexPos = m_HasVertexPos;

#ifdef THREADING_ENABLED
	const uint32 numThreads = CMaths::Min((chunkSize > 1) ? 2u : 1u, chunkSize);
	//if (numThreads == 0) numThread = 4; // fallback
	//numThread = 2; // match PS3
	if (numThreads > 1)
	{
		BatchThreadData_t batchThreadData[2];
		pthread_t threads[2];
		const uint32 batchesPerThread = (chunkSize + numThreads - 1) / numThreads;

		for (uint32 i = 0; i < numThreads; i++)
		{
			const uint32 threadStart = chunkStart + i * batchesPerThread;
			const uint32 threadEnd = CMaths::Min(
				threadStart + batchesPerThread,
				chunkStart + chunkSize);
			if (threadStart >= chunkStart + chunkSize) break;

			batchThreadData[i].m_pDstVertexData = pDstVertexData;
			batchThreadData[i].m_pDstIndexData = pDstIndexData;
			batchThreadData[i].m_pSrcVertexData = pSrcVertexData;
			batchThreadData[i].m_pSrcIndices = pSrcIndices;
			batchThreadData[i].m_pBatchChunkTransforms = &batchChunkTransforms;
			batchThreadData[i].m_ThreadStart = threadStart;
			batchThreadData[i].m_ThreadEnd = threadEnd;
			batchThreadData[i].m_ChunkStart = chunkStart;
			batchThreadData[i].m_VertexCount = vertexCount;
			batchThreadData[i].m_IndexCount = indexCount;
			batchThreadData[i].m_VertexLayoutStride = vertexStride;
			batchThreadData[i].m_VertexPosOffset = vertexPosOffset;
			batchThreadData[i].m_HasVertexPos = hasVertexPos;

			int32 threadCreateResult = pthread_create(
				&threads[i],
				GCMGL_NULL,
				BatchIndexedTransforms,
				&batchThreadData[i]);
			if (threadCreateResult != 0)
			{
				Warning(
					"[GlBatchRenderer] Failed to create thread %d: %d\n",
					i,
					threadCreateResult);
			}
		}

		for (uint32 i = 0; i < numThreads; i++)
		{
			pthread_join(threads[i], GCMGL_NULL);
		}
	}
#endif // THREADING_ENABLED
#ifndef THREADING_ENABLED
	{
		for (uint32 i = 0; i < chunkSize; i++)
		{
			char* pBatchVertexDst = pDstVertexData + uint64(i) * vertexCount * vertexStride;
			uint32* pBatchIndexDst = pDstIndexData + uint64(i) * indexCount;
			memcpy(
				pBatchVertexDst,
				pSrcVertexData,
				uint64(vertexCount) * vertexStride);

			if (hasVertexPos)
			{
				TransformVertices(
					pBatchVertexDst,
					vertexCount,
					vertexStride,
					vertexPosOffset,
					batchChunkTransforms[chunkStart + i].ToMatrix());
			}

			const uint32 vertexBase = i * vertexCount;
			for (uint32 j = 0; j < indexCount; j++)
			{
				pBatchIndexDst[j] = pSrcIndices[j] + vertexBase;
			}
		}
	}
#endif // !THREADING_ENABLED

	if (stagingVertexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingVertexBuffer.m_hId);
	}

	glBindBuffer(GL_ARRAY_BUFFER, stagingVertexBuffer.m_hId);
	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalVertexDataSize),
		stagingVertexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	if (stagingIndexBuffer.m_hId == 0)
	{
		glGenBuffers(1, &stagingIndexBuffer.m_hId);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stagingIndexBuffer.m_hId);
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalIndexDataSize),
		stagingIndexBuffer.m_Data.Base(),
		GL_STREAM_DRAW);

	const BufferResource_t stagingVertexBufferResource = {
		totalVertexDataSize,
		stagingVertexBuffer.m_Data.Base(),
		stagingVertexBuffer.m_hId,
		GL_ARRAY_BUFFER,
		false
	};
	m_BufferResources.Insert(
		stagingVertexBuffer.m_hBuffer,
		stagingVertexBufferResource);

	const BufferResource_t stagingIndexBufferResource = {
		totalIndexDataSize,
		stagingIndexBuffer.m_Data.Base(),
		stagingIndexBuffer.m_hId,
		GL_ELEMENT_ARRAY_BUFFER,
		false
	};
	m_BufferResources.Insert(
		stagingIndexBuffer.m_hBuffer,
		stagingIndexBufferResource);

	uint64 originalIndexOffset = m_PipelineState.m_IndexOffset;
	BufferHandle hOriginalVertexBuffer = m_PipelineState.m_hVertexBuffer;
	BufferHandle hOriginalIndexBuffer = m_PipelineState.m_hIndexBuffer;
	uint32 originalVertexOffset = m_PipelineState.m_VertexOffset;

	m_PipelineState.m_IndexOffset = 0;
	m_PipelineState.m_hVertexBuffer = stagingVertexBuffer.m_hBuffer;
	m_PipelineState.m_hIndexBuffer = stagingIndexBuffer.m_hBuffer;
	m_PipelineState.m_VertexOffset = 0;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	DrawIndexed(totalIndices);

	m_PipelineState.m_IndexOffset = originalIndexOffset;
	m_PipelineState.m_hVertexBuffer = hOriginalVertexBuffer;
	m_PipelineState.m_hIndexBuffer = hOriginalIndexBuffer;
	m_PipelineState.m_VertexOffset = originalVertexOffset;
	m_StateDirtyFlags = m_StateDirtyFlags |
		StateDirtyFlags_t::VertexBuffer |
		StateDirtyFlags_t::IndexBuffer;

	m_StagingIndex = (m_StagingIndex + 1) & 1;
}