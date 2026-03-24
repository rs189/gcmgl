# gcmgl
<img src="media/gcmgl.png" height="180"/>

### A C++ graphics library for PlayStation 3 and Linux.

gcmgl is a C++ graphics library targeting PlayStation 3 (GCM) and Linux (x86_64, OpenGL). gcmgl provides a platform-abstracted renderer interface covering viewports, buffers, shader programs, textures, blend states, draw calls, and batch rendering. Mathematics support is provided by [mathsfury](https://github.com/rs189/mathsfury).

## Requirements

##### Core dependencies:

- [mathsfury](https://github.com/rs189/mathsfury)

##### PS3 build dependencies:

- [ps3toolchain](http://github.com/ps3dev/ps3toolchain)
- [PSL1GHT](http://github.com/ps3dev/PSL1GHT)
- [NVIDIA Cg Toolkit](https://developer.nvidia.com/cg-toolkit-download)
- [ImageMagick](https://imagemagick.org)

##### Linux build dependencies:

- [glad](https://github.com/Dav1dde/glad)
- [SIMDe](https://github.com/simd-everywhere/simde) (optional, for SSE path)

## Interface

### Handles
- `BufferHandle`
- `ShaderProgramHandle`
- `TextureHandle`
- `SamplerHandle`
- `UniformBlockLayoutHandle`

### ClearFlags_t
- `ClearNone`
- `ClearColor`
- `ClearDepth`
- `ClearStencil`
- `ClearAll`

### BufferUsage_t
- `Static`
- `Dynamic`
- `Immutable`

### IndexFormat_t
- `UInt16`
- `UInt32`

### TextureFormat_t
- `R8`
- `RG8`
- `RGB8`
- `RGBA8`
- `R16F`
- `RG16F`
- `RGB16F`
- `RGBA16F`
- `R32F`
- `RG32F`
- `RGB32F`
- `RGBA32F`
- `Depth16`
- `Depth24`
- `Depth32F`
- `Depth24Stencil8`

### ShaderStage_t
- `ShaderStageVertex`
- `ShaderStageFragment`
- `ShaderStageAll`

### VertexFormat_t
- `Float`
- `Float2`
- `Float3`
- `Float4`
- `UByte4_Norm`

### VertexSemantic_t
- `Position`
- `Normal`
- `Color0`
- `Color1`
- `TexCoord0`
- `TexCoord1`
- `TexCoord2`
- `TexCoord3`
- `TexCoord4`
- `TexCoord5`
- `TexCoord6`
- `TexCoord7`
- `Tangent`
- `Binormal`

### RendererDesc_t
- `m_Width`
- `m_Height`
- `m_IsFullscreen`
- `m_IsVSync`
- `m_pWindow`

### Viewport_t
- `m_X`
- `m_Y`
- `m_Width`
- `m_Height`
- `m_MinDepth`
- `m_MaxDepth`

### Rect_t
- `m_X`
- `m_Y`
- `m_Width`
- `m_Height`

### BlendState_t
- `m_IsEnabled`

### DepthStencilState_t
- `m_IsDepthTest`
- `m_IsDepthWrite`

### PipelineState_t
- `m_hShaderProgram`
- `m_hVertexBuffer`
- `m_hIndexBuffer`
- `m_pVertexLayout`
- `m_VertexStride`
- `m_VertexOffset`
- `m_IndexOffset`
- `m_BlendState`
- `m_DepthStencilState`

### UniformBlockLayout_t
- `m_UniformNames`
- `m_Binding`
- `m_Size`

### VertexAttribute_t
- `m_Name`
- `m_Format`
- `m_Offset`
- `m_Location`
- `m_Semantic`

### Plane_t
- `m_Normal`
- `m_Distance`

### BatchTransform_t
- `m_Position`
- `m_Rotation`
- `m_Scale`
- `ToMatrix()`

### BatchData_t
- `AddBatch(const CVector3& pos, const CQuaternion& rot, const CVector3& scale)`
- `Clear()`
- `GetCount()`

### CVertexLayout
- `AddAttribute(name, format, offset, location)`
- `AddAttribute(name, format, offset, semantic, location)`
- `SetStride(uint32 stride)`
- `GetStride()`
- `GetAttributes()`

### IRenderer / CRenderer
- `Init(const RendererDesc_t& desc)`
- `Shutdown()`
- `SetEnvironment()`
- `BeginFrame()`
- `EndFrame()`
- `Clear(uint32 clearFlags, const CColor& color, float32 depth, uint32 stencil)`
- `SetViewport(const Viewport_t& viewport)`
- `SetScissor(const Rect_t& rect)`
- `SetStencilRef(uint32 stencilRef)`
- `CreateVertexBuffer(const void* pData, uint64 size, BufferUsage_t usage)`
- `CreateIndexBuffer(const void* pData, uint64 size, IndexFormat_t format, BufferUsage_t usage)`
- `CreateConstantBuffer(uint64 size, BufferUsage_t usage)`
- `UpdateBuffer(BufferHandle hBuffer, const void* pData, uint64 size, uint64 offset)`
- `DestroyBuffer(BufferHandle hBuffer)`
- `MapBuffer(BufferHandle hBuffer)`
- `UnmapBuffer(BufferHandle hBuffer)`
- `CreateStagingBuffer(uint64 size)`
- `CreateShaderProgram(const CFixedString& shaderName)`
- `GetOrCreateShaderProgram(const CFixedString& shaderName)`
- `DestroyShaderProgram(ShaderProgramHandle hProgram)`
- `ClearShaderCache()`
- `CreateTexture2D(uint32 width, uint32 height, TextureFormat_t format, const void* pData)`
- `CreateTextureCube(uint32 size, TextureFormat_t format, const void** ppFaces)`
- `SetTexture(TextureHandle hTexture, uint32 slot, ShaderStage_t stage)`
- `SetSampler(SamplerHandle hSampler, uint32 slot, ShaderStage_t stage)`
- `UpdateTexture(TextureHandle hTexture, const void* pData, uint32 mipLevel)`
- `DestroyTexture(TextureHandle hTexture)`
- `SetShaderProgram(ShaderProgramHandle hProgram)`
- `SetVertexBuffer(BufferHandle hBuffer, uint32 slot, uint32 stride, uint32 offset, const CVertexLayout* pLayout)`
- `SetIndexBuffer(BufferHandle hBuffer, uint64 offset)`
- `CreateUniformBlockLayout(const UniformBlockLayout_t& layout)`
- `SetConstantBuffer(BufferHandle hBuffer, UniformBlockLayoutHandle hLayout, uint32 slot, ShaderStage_t stage)`
- `SetBlendState(const BlendState_t& state)`
- `SetDepthStencilState(const DepthStencilState_t& state)`
- `ApplyVertexConstants(ShaderProgramHandle hProgram)`
- `ApplyFragmentConstants(ShaderProgramHandle hProgram)`
- `Draw(uint32 vertexCount, uint32 startVertex, const CMatrix4* pViewProjection, const CVector3* pAABBCenter, const CVector3* pAABBExtent)`
- `DrawIndexed(uint32 indexCount, uint32 startIndex, int32 baseVertex, const CMatrix4* pViewProjection, const CVector3* pAABBCenter, const CVector3* pAABBExtent)`
- `DrawBatched(uint32 vertexCount, const BatchData_t& batchData, const CMatrix4& viewProjection, uint32 startVertex)`
- `DrawIndexedBatched(uint32 indexCount, uint32 vertexCount, const BatchData_t& batchData, const CMatrix4& viewProjection, uint32 startIndex, int32 baseVertex)`
- `SetPipelineState(const PipelineState_t& state)`
- `FlushPipelineState()`
- `ExtractFrustumPlanes(const CMatrix4& mvp, Plane_t* pPlanes)`
- `TestAABBFrustum(const CVector3& center, const CVector3& extent, const Plane_t* pPlanes)`

### CBatchRenderer
- `DrawBatched(uint32 vertexCount, const BatchData_t& batchData, const CMatrix4& viewProjection, uint32 startVertex)`
- `DrawIndexedBatched(uint32 indexCount, uint32 vertexCount, const BatchData_t& batchData, const CMatrix4& viewProjection, uint32 startIndex, int32 baseVertex)`

## Build

Install required build dependencies:

- Fedora:
```bash
sudo dnf install cmake gcc-c++ glfw-devel mesa-libGL-devel pkgconf-pkg-config ImageMagick
```

Build using the provided scripts:

- Linux (x86_64):
```bash
./build_linux_x86_64.sh
```
- PlayStation 3:
```bash
./build_ps3.sh
```

## Examples

Build scripts prompt for an example to build: `Triangle`, `Cube`, `Shader`, `Textured`, `Lit`, `TexturedLit`, or `Batch`.

- Triangle
![Triangle](media/Triangle.png)
- Cube
![Cube](media/Cube.png)
- Shader
![Shader](media/Shader.png)
- Textured
![Textured](media/Textured.png)
- Lit
![Lit](media/Lit.png)
- TexturedLit
![TexturedLit](media/TexturedLit.png)
- Batch
![Batch](media/Batch.png)

## License
gcmgl is licensed under the [MIT License](LICENSE).