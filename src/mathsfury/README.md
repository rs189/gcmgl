# mathsfury
<img src="mathsfury.png" height="180"/>

### A C++ mathematics library for 3D graphics.

mathsfury is a C++ mathematics library for 3D graphics targeting PlayStation 3 and Linux (x86_64). mathsfury provides vectors, quaternions, and matrices as well as common mathematical functions. It covers graphics programming use cases, including transformations, camera manipulation, and projection.

## Requirements

##### Core dependencies:

- [`public/tier0/platform.h`](https://gist.github.com/rs189/0a5cd6dca531814218f2e31dab0556ca) (internal platform definitions)
- [`public/tier0/dbg.h`](https://gist.github.com/rs189/77924631ac85ab5092a7aa7dccb11b48) (debug definitions)

##### PS3 Build dependencies:

- [ps3toolchain](http://github.com/ps3dev/ps3toolchain)
- [PSL1GHT](http://github.com/ps3dev/PSL1GHT)

##### Linux build dependencies (optional, for SSE path):

- [SIMDe](https://github.com/simd-everywhere/simde)

## Interface

### CMaths
- `static float32 Sqrt(float32 f)`
- `static float32 Tan(float32 f)`
- `static float32 Cos(float32 f)`
- `static float32 Sin(float32 f)`
- `static float32 ATan2(float32 y, float32 x)`
- `static float32 ASin(float32 f)`
- `static float32 Abs(float32 a)`
- `static int32 Abs(int32 a)`
- `static float32 Min(float32 a, float32 b)`
- `static int32 Min(int32 a, int32 b)`
- `static uint32 Min(uint32 a, uint32 b)`
- `static float32 Max(float32 a, float32 b)`
- `static int32 Max(int32 a, int32 b)`
- `static uint32 Max(uint32 a, uint32 b)`
- `static float32 Clamp(float32 value, float32 minVal, float32 maxVal)`
- `static int32 Clamp(int32 value, int32 minVal, int32 maxVal)`
- `static uint32 Clamp(uint32 value, uint32 minVal, uint32 maxVal)`
- `static CVector3 Normalise(const CVector3& v)`
- `static CVector3 Cross(const CVector3& a, const CVector3& b)`
- `static float32 Dot(const CVector3& a, const CVector3& b)`
- `static CMatrix4 Perspective(float32 fovyDeg, float32 aspect, float32 nearClip, float32 farClip)`
- `static CMatrix4 Orthographic(float32 left, float32 right, float32 bottom, float32 top, float32 nearClip, float32 farClip)`
- `static CMatrix4 LookAt(const CVector3& eye, const CVector3& target, const CVector3& up)`
- `static CMatrix4 Translate(const CMatrix4& m, const CVector3& v)`
- `static CMatrix4 Rotate(const CMatrix4& m, float32 angleDeg, const CVector3& axis)`
- `static CMatrix4 Scale(const CMatrix4& m, const CVector3& v)`
- `static EquirectUV_t DirectionToEquirect(const CVector3& dir)`

### CVector2(float32 m_X, float32 m_Y)
- `CVector2 operator+(const CVector2& other) const`
- `CVector2 operator-(const CVector2& other) const`
- `CVector2 operator*(const CVector2& other) const`
- `CVector2 operator*(float32 scalar) const`
- `CVector2 operator/(const CVector2& other) const`
- `CVector2 operator/(float32 scalar) const`
- `CVector2& operator+=(const CVector2& other)`
- `CVector2& operator-=(const CVector2& other)`
- `CVector2& operator*=(const CVector2& other)`
- `CVector2& operator*=(float32 scalar)`
- `CVector2& operator/=(const CVector2& other)`
- `CVector2& operator/=(float32 scalar)`
- `bool operator==(const CVector2& other) const`
- `bool operator!=(const CVector2& other) const`
- `float32& operator[](int32 index)`
- `float32 Length() const`
- `float32 LengthSq() const`
- `float32 Distance(const CVector2& other) const`
- `float32 DistanceSq(const CVector2& other) const`

### CVector3(float32 m_X, float32 m_Y, float32 m_Z)
- `CVector3 operator+(const CVector3& other) const`
- `CVector3 operator-(const CVector3& other) const`
- `CVector3 operator*(const CVector3& other) const`
- `CVector3 operator*(float32 scalar) const`
- `CVector3 operator/(const CVector3& other) const`
- `CVector3 operator/(float32 scalar) const`
- `CVector3& operator+=(const CVector3& other)`
- `CVector3& operator-=(const CVector3& other)`
- `CVector3& operator*=(const CVector3& other)`
- `CVector3& operator*=(float32 scalar)`
- `CVector3& operator/=(const CVector3& other)`
- `CVector3& operator/=(float32 scalar)`
- `bool operator==(const CVector3& other) const`
- `bool operator!=(const CVector3& other) const`
- `float32& operator[](int32 index)`
- `float32 LengthXY() const`
- `float32 Length() const`
- `float32 LengthSq() const`
- `float32 Distance(const CVector3& other) const`
- `float32 DistanceSq(const CVector3& other) const`
- `float32 Dot(const CVector3& other) const`

### CVector4(float32 m_X, float32 m_Y, float32 m_Z, float32 m_W)
- `CVector4 operator+(const CVector4& other) const`
- `CVector4 operator-(const CVector4& other) const`
- `CVector4 operator*(const CVector4& other) const`
- `CVector4 operator*(float32 scalar) const`
- `CVector4 operator/(const CVector4& other) const`
- `CVector4 operator/(float32 scalar) const`
- `CVector4& operator+=(const CVector4& other)`
- `CVector4& operator-=(const CVector4& other)`
- `CVector4& operator*=(const CVector4& other)`
- `CVector4& operator*=(float32 scalar)`
- `CVector4& operator/=(const CVector4& other)`
- `CVector4& operator/=(float32 scalar)`
- `bool operator==(const CVector4& other) const`
- `bool operator!=(const CVector4& other) const`
- `float32& operator[](int32 index)`
- `float32 Length() const`
- `float32 LengthSq() const`
- `float32 Distance(const CVector4& other) const`
- `float32 DistanceSq(const CVector4& other) const`

### CQuaternion(float32 m_X, float32 m_Y, float32 m_Z, float32 m_W)
- `CQuaternion operator*(const CQuaternion& other) const`
- `CMatrix4 ToMatrix() const`
- `CMatrix4 ToTransformMatrix(const CVector3& position, const CVector3& scale) const`
- `CQuaternion& Normalise()`
- `float32 Length() const`
- `static CQuaternion FromEuler(float32 pitch, float32 yaw, float32 roll)`

### CMatrix4(float32 m_Data[16])
- `CMatrix4 operator+(const CMatrix4& other) const`
- `CMatrix4 operator-(const CMatrix4& other) const`
- `CMatrix4 operator*(const CMatrix4& other) const`
- `CMatrix4& operator+=(const CMatrix4& other)`
- `CMatrix4& operator-=(const CMatrix4& other)`
- `CMatrix4& operator*=(const CMatrix4& other)`
- `CVector4 operator*(const CVector4& v) const`
- `bool operator==(const CMatrix4& other) const`
- `bool operator!=(const CMatrix4& other) const`
- `CMatrix4 ToTransformMatrix(const CVector3& position, const CVector3& scale) const`

## Usage

```cpp
#include "Maths.h"

CVector3 position(1.0f, 2.0f, 3.0f);
CVector3 eye(0.0f, 0.0f, 5.0f);
CVector3 target(0.0f, 0.0f, 0.0f);
CVector3 up(0.0f, 1.0f, 0.0f);

// Transformation matrices
CMatrix4 identity;
CMatrix4 transform = CMaths::Translate(identity, position);
CMatrix4 rotated = CMaths::Rotate(transform, 45.0f, CVector3(0.0f, 1.0f, 0.0f));
CMatrix4 scaled = CMaths::Scale(identity, CVector3(2.0f, 2.0f, 2.0f));

// Camera and projection
CMatrix4 view = CMaths::LookAt(eye, target, up);
CMatrix4 projection = CMaths::Perspective(90.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
CMatrix4 ortho = CMaths::Orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

// Vector operations
CVector3 a(1.0f, 0.0f, 0.0f);
CVector3 b(0.0f, 1.0f, 0.0f);
CVector3 norm = CMaths::Normalise(position);
CVector3 cross = CMaths::Cross(a, b);
float32 dot = CMaths::Dot(a, b);

// Quaternion rotation
CQuaternion rotation = CQuaternion::FromEuler(0.0f, 45.0f, 0.0f);
CMatrix4 rotationTransform = rotation.ToTransformMatrix(position, CVector3(1.0f, 1.0f, 1.0f));
```

## License
mathsfury is licensed under the [MIT License](LICENSE).
