

RWStructuredBuffer<float4> render_target : register(u1);


[RootSignature("RootFlags(0), UAV(u1)")]
[numthreads(32,1,1)]
void main(
    uint3 dispatchThreadID : SV_DispatchThreadID
)
{
    uint threadID = dispatchThreadID.x;
    
    render_target[threadID] = float4(1.0, 0.0, 0.0, 1.0);
}


