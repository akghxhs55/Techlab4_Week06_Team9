#pragma once

// TODO: 기존 연산으로 가능한 부분들은 빼기
namespace ViewMath
{
    // float 무한대 대응은 유한 최댓값으로 대체하지 않는다.
    static_assert(std::numeric_limits<float>::has_infinity, "Float type must support infinity");

    inline constexpr float Pi = 3.14159265358979323846f;
    inline constexpr float Epsilon = 1.0e-6f;

    // 두 벡터의 각 성분을 더한다.
    inline FVector Add(const FVector A, const FVector B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
    // 두 벡터의 각 성분을 뺀다.
    inline FVector Subtract(const FVector A, const FVector B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
    // 벡터의 각 성분에 스칼라를 곱한다.
    inline FVector Scale(const FVector V, const float S) { return { V.X * S, V.Y * S, V.Z * S }; }
    // 두 벡터의 내적을 계산한다.
    inline float Dot(const FVector A, const FVector B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
    // 오른손 좌표계의 벡터 외적을 계산한다.
    inline FVector Cross(const FVector A, const FVector B)
    {
        return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X };
    }
    // 제곱근 없이 벡터 길이의 제곱을 계산한다.
    inline float LengthSquared(const FVector V) { return Dot(V, V); }
    // 0이 아닌 벡터를 길이 1로 정규화한다.
    inline FVector Normalize(const FVector V)
    {
        const float Length = sqrtf(LengthSquared(V));
        assert(Length > Epsilon);
        return Scale(V, 1.0f / Length);
    }

    // 0이 아닌 quaternion을 길이 1로 정규화한다.
    inline FQuat Normalize(const FQuat Q)
    {
        const float Length = sqrtf(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
        assert(Length > Epsilon);
        return { Q.X / Length, Q.Y / Length, Q.Z / Length, Q.W / Length };
    }

    // Hamilton product로 두 quaternion 회전을 합성한다.
    inline FQuat Multiply(const FQuat A, const FQuat B)
    {
        return {
            A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
            A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
            A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
            A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z };
    }

    // 단위 축과 각도로 회전 quaternion을 만든다.
    inline FQuat AxisAngle(const FVector Axis, const float Radians)
    {
        const float Half = Radians * 0.5f;
        const float Sine = sinf(Half);
        const FVector UnitAxis = Normalize(Axis);
        return { UnitAxis.X * Sine, UnitAxis.Y * Sine, UnitAxis.Z * Sine, cosf(Half) };
    }

    // 정규화 quaternion의 벡터 회전 공식을 사용해 방향을 회전한다.
    inline FVector Rotate(const FQuat Rotation, const FVector V)
    {
        const FQuat Q = Normalize(Rotation);
        const FVector U{ Q.X, Q.Y, Q.Z };
        return Add(Add(Scale(U, 2.0f * Dot(U, V)), Scale(V, Q.W * Q.W - Dot(U, U))), Scale(Cross(U, V), 2.0f * Q.W));
    }

    // 모든 원소가 0인 4x4 행렬을 만든다.
    inline FMatrix ZeroMatrix()
    {
        FMatrix Result;
        for (int Row = 0; Row < 4; ++Row)
            for (int Column = 0; Column < 4; ++Column)
                Result.M[Row][Column] = 0.0f;
        return Result;
    }
}
