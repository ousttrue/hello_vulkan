#include <DirectXMath.h>

#include <cuber/dx/DxLineRenderer.h>
#include <cuber/mesh.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <grapho/dx11/shader.h>

static auto SHADER = R"(
#pragma pack_matrix(row_major)
float4x4 VP;
struct vs_in {
    float3 pos: POSITION;
    float4 color: COLOR;
};
struct vs_out {
    float4 position_clip: SV_POSITION;
    float4 color: COLOR;
};

vs_out vs_main(vs_in IN) {
  vs_out OUT = (vs_out)0; // zero the memory first
  OUT.position_clip = mul(float4(IN.pos, 1.0), VP);
  OUT.color = IN.color;
  return OUT;
}

float4 ps_main(vs_out IN) : SV_TARGET {
  return IN.color;
}
)";

namespace cuber::dx11 {

struct DxLineRendererImpl
{
  winrt::com_ptr<ID3D11Device> device_;
  winrt::com_ptr<ID3D11VertexShader> vertex_shader_;
  winrt::com_ptr<ID3D11PixelShader> pixel_shader_;

  winrt::com_ptr<ID3D11InputLayout> input_layout_;
  winrt::com_ptr<ID3D11Buffer> vertex_buffer_;
  winrt::com_ptr<ID3D11Buffer> constant_buffer_;

  DxLineRendererImpl(const winrt::com_ptr<ID3D11Device>& device)
    : device_(device)
  {
    auto vs = grapho::dx11::CompileShader(SHADER, "vs_main", "vs_5_0");
    if (!vs) {
      OutputDebugStringA(vs.error().c_str());
    }
    auto hr = device_->CreateVertexShader((*vs)->GetBufferPointer(),
                                          (*vs)->GetBufferSize(),
                                          NULL,
                                          vertex_shader_.put());
    assert(SUCCEEDED(hr));

    auto ps = grapho::dx11::CompileShader(SHADER, "ps_main", "ps_5_0");
    if (!ps) {
      OutputDebugStringA(ps.error().c_str());
    }
    hr = device_->CreatePixelShader((*ps)->GetBufferPointer(),
                                    (*ps)->GetBufferSize(),
                                    NULL,
                                    pixel_shader_.put());
    assert(SUCCEEDED(hr));

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[]{
      {
        .SemanticName = "POSITION",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32_FLOAT,
        .InputSlot = 0,
        .AlignedByteOffset = 0,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0,
      },
      {
        .SemanticName = "COLOR",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .InputSlot = 0,
        .AlignedByteOffset = offsetof(LineVertex, Color),
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0,
      },
    };
    hr = device_->CreateInputLayout(inputElementDesc,
                                    std::size(inputElementDesc),
                                    (*vs)->GetBufferPointer(),
                                    (*vs)->GetBufferSize(),
                                    input_layout_.put());
    assert(SUCCEEDED(hr));

    {
      D3D11_BUFFER_DESC vertex_buff_desc = {
        .ByteWidth = static_cast<uint32_t>(sizeof(Vertex) * 65535),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
      };
      hr =
        device_->CreateBuffer(&vertex_buff_desc, nullptr, vertex_buffer_.put());
      assert(SUCCEEDED(hr));
    }

    {
      D3D11_BUFFER_DESC desc = {
        .ByteWidth = sizeof(DirectX::XMFLOAT4X4),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
      };
      HRESULT hr =
        device_->CreateBuffer(&desc, nullptr, constant_buffer_.put());
      assert(SUCCEEDED(hr));
    };
  }

  void Render(const float projection[16],
              const float view[16],
              std::span<const LineVertex> lines)
  {
    if (lines.empty()) {
      return;
    }
    winrt::com_ptr<ID3D11DeviceContext> context;
    device_->GetImmediateContext(context.put());

    // shader
    context->VSSetShader(vertex_shader_.get(), NULL, 0);
    context->PSSetShader(pixel_shader_.get(), NULL, 0);

    // constant buffer
    auto v = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)view);
    auto p = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)projection);
    DirectX::XMFLOAT4X4 vp;
    DirectX::XMStoreFloat4x4(&vp, v * p);
    context->UpdateSubresource(constant_buffer_.get(), 0, NULL, &vp, 0, 0);
    ID3D11Buffer* cb[]{ constant_buffer_.get() };
    context->VSSetConstantBuffers(0, 1, cb);

    // vertices
    D3D11_BOX box{
      .left = 0,
      .top = 0,
      .front = 0,
      .right = static_cast<uint32_t>(sizeof(LineVertex) * lines.size()),
      .bottom = 1,
      .back = 1,
    };
    context->UpdateSubresource(
      vertex_buffer_.get(), 0, &box, lines.data(), 0, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->IASetInputLayout(input_layout_.get());
    ID3D11Buffer* vb[] = {
      vertex_buffer_.get(),
    };
    uint32_t strides[] = {
      sizeof(LineVertex),
    };
    uint32_t offsets[] = {
      0,
    };
    context->IASetVertexBuffers(0, std::size(vb), vb, strides, offsets);
    context->Draw(lines.size(), 0);
  }
};

DxLineRenderer::DxLineRenderer(const winrt::com_ptr<ID3D11Device>& device)
  : impl_(new DxLineRendererImpl(device))
{
}
DxLineRenderer::~DxLineRenderer()
{
  delete impl_;
}
void
DxLineRenderer::Render(const float projection[16],
                       const float view[16],
                       std::span<const LineVertex> lines)
{
  impl_->Render(projection, view, lines);
}

} // namespace cuber::dx11
