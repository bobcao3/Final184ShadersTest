#include <PresentationSurfaceDesc.h>
#include <RHIImGuiBackend.h>
#include <RHIInstance.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <Windows.h>
#include <fstream>

#include <chrono>
#include <thread>

#include <glm/vec2.hpp> // glm::vec2
#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure

#include "imgui.h"
#include "imgui_impl_sdl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace RHI;

// ============================================================================
//  LoadSPIRV : Load compiled SPRIV shaders
// ============================================================================
static CShaderModule::Ref LoadSPIRV(CDevice::Ref device, const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size))
        return device->CreateShaderModule(buffer.size(), buffer.data());
    return {};
}

// ============================================================================
//  CreateScreenPass : Create Scene Render Pass
// ============================================================================
static CRenderPass::Ref CreateScreenPass(CDevice::Ref device, CSwapChain::Ref swapChain)
{
    auto fbImage = swapChain->GetImage();
    uint32_t width, height;
    swapChain->GetSize(width, height);

    CImageViewDesc fbViewDesc;
    fbViewDesc.Format = EFormat::R8G8B8A8_UNORM;
    fbViewDesc.Type = EImageViewType::View2D;
    fbViewDesc.Range.Set(0, 1, 0, 1);
    auto fbView = device->CreateImageView(fbViewDesc, fbImage);

    auto depthImage = device->CreateImage2D(EFormat::D24_UNORM_S8_UINT,
        EImageUsageFlags::DepthStencil, width, height);
    CImageViewDesc depthViewDesc;
    depthViewDesc.Format = EFormat::D24_UNORM_S8_UINT;
    depthViewDesc.Type = EImageViewType::View2D;
    depthViewDesc.Range.Set(0, 1, 0, 1);
    auto depthView = device->CreateImageView(depthViewDesc, depthImage);

    CRenderPassDesc rpDesc;
    rpDesc.AddAttachment(fbView, EAttachmentLoadOp::Clear, EAttachmentStoreOp::Store);
    rpDesc.AddAttachment(depthView, EAttachmentLoadOp::Clear, EAttachmentStoreOp::DontCare);
    rpDesc.Subpasses.resize(1);
    rpDesc.Subpasses[0].AddColorAttachment(0);
    rpDesc.Subpasses[0].SetDepthStencilAttachment(1);
    swapChain->GetSize(rpDesc.Width, rpDesc.Height);
    rpDesc.Layers = 1;
    return device->CreateRenderPass(rpDesc);
}

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct ShadersUniform {
    glm::vec4 color;
    glm::mat4 projection;
    glm::mat4 modelview;
};

// ============================================================================
//  Physics & game logic thread
// ============================================================================
bool terminated = false;

const int tickRate = 120;
const double tickInterval = 1.0 / (double) tickRate;

using namespace std::chrono_literals;

void game_logic(CPipeline::Ref pso, CBuffer::Ref ubo) {
    uint64_t ticksElapsed = 0;
    auto timeStart = std::chrono::steady_clock::now();

    while (!terminated) {
        auto timeTarget = std::chrono::steady_clock::now() + std::chrono::duration<double>(tickInterval);
        double elapsedTime = (std::chrono::steady_clock::now() - timeStart).count() * 1.0e-9;

        ShadersUniform* uniform = static_cast<ShadersUniform*>(ubo->Map(0, sizeof(ShadersUniform)));
        uniform->color = { std::sin(elapsedTime), std::cos(elapsedTime), 1.0f, 1.0f };
        uniform->modelview = glm::lookAt(
            glm::vec3(std::sin(elapsedTime), 0.0f, std::cos(elapsedTime)),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        ubo->Unmap();

        ticksElapsed++;
        std::this_thread::sleep_until(timeTarget);
    }
}

// ============================================================================
//  main
// ============================================================================
int main(int argc, char* argv[])
{
    // Initialize devices & windows
    auto device = CInstance::Get().CreateDevice(EDeviceCreateHints::NoHint);
    SDL_Window* window;
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("RHI Triangle Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    // Bind presentation surface to window
    CPresentationSurfaceDesc surfaceDesc;
    surfaceDesc.Type = EPresentationSurfaceDescType::Win32;
    surfaceDesc.Win32.Instance = wmInfo.info.win.hinstance;
    surfaceDesc.Win32.Window = wmInfo.info.win.window;
    auto swapChain = device->CreateSwapChain(surfaceDesc, EFormat::R8G8B8A8_UNORM);

    auto screenPass = CreateScreenPass(device, swapChain);

    // Setup ImGui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForVulkan(window);
    CRHIImGuiBackend::Init(device, screenPass);

    // Setup pipeline
    CPipeline::Ref pso;
    {
        CPipelineDesc pipelineDesc;
        CRasterizerDesc rastDesc;
        CDepthStencilDesc depthStencilDesc;
        CBlendDesc blendDesc;
        rastDesc.CullMode = ECullModeFlags::None;
        pipelineDesc.VS = LoadSPIRV(device, APP_SOURCE_DIR "/Shader/Demo1.vert.spv");
        pipelineDesc.PS = LoadSPIRV(device, APP_SOURCE_DIR "/Shader/Demo2.frag.spv");
        pipelineDesc.RasterizerState = &rastDesc;
        pipelineDesc.DepthStencilState = &depthStencilDesc;
        pipelineDesc.BlendState = &blendDesc;
        pipelineDesc.RenderPass = screenPass;

        CVertexInputAttributeDesc vertInputDescVertices = {
            0,
            EFormat::R32G32B32_SFLOAT,
            offsetof(Vertex, pos),
            0
        };

        CVertexInputAttributeDesc vertInputDescColors = {
            1,
            EFormat::R32G32B32_SFLOAT,
            offsetof(Vertex, color),
            0
        };

        CVertexInputBindingDesc vertInputBinding = {
            0,
            sizeof(Vertex),
            false
        };


        pipelineDesc.VertexAttributes = { vertInputDescVertices, vertInputDescColors };
        pipelineDesc.VertexBindings = { vertInputBinding };

        pso = device->CreatePipeline(pipelineDesc);
    }

    // Setup uniforms
    auto ubo = device->CreateBuffer(sizeof(ShadersUniform), EBufferUsageFlags::ConstantBuffer);
    ShadersUniform* uniform = static_cast<ShadersUniform*>(ubo->Map(0, sizeof(ShadersUniform)));
    uniform->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    uniform->projection = glm::perspective(
        glm::radians(70.0),
        640.0 / 480.0,
        0.01,
        256.0
    );
    uniform->modelview = glm::mat4(1.0);
    ubo->Unmap();

    // Setup VBO
    Vertex vertices[] = {
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };
    auto vbo = device->CreateBuffer(3 * sizeof(Vertex), EBufferUsageFlags::VertexBuffer, &vertices);

    // Setup texture
    int x, y, comp;
    auto* checker512Data = stbi_load(APP_SOURCE_DIR "/checker512.png", &x, &y, &comp, 4);
    auto checker512 = device->CreateImage2D(EFormat::R8G8B8A8_UNORM, EImageUsageFlags::Sampled, 512,
        512, 1, 1, 1, checker512Data);
    stbi_image_free(checker512Data);
    CImageViewDesc checkerViewDesc;
    checkerViewDesc.Format = EFormat::R8G8B8A8_UNORM;
    checkerViewDesc.Type = EImageViewType::View2D;
    checkerViewDesc.Range.Set(0, 1, 0, 1);
    auto checkerView = device->CreateImageView(checkerViewDesc, checker512);

    CSamplerDesc samplerDesc;
    auto sampler = device->CreateSampler(samplerDesc);

    // Main render loop
    auto ctx = device->GetImmediateContext();

    std::thread physicsThread(game_logic, pso, ubo);

    while (!terminated)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                terminated = true;
        }

        // Draw GUI stuff
        CRHIImGuiBackend::NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        // swapchain stuff
        bool swapOk = swapChain->AcquireNextImage();
        if (!swapOk)
        {
            screenPass.reset();
            swapChain->Resize(UINT32_MAX, UINT32_MAX);
            screenPass = CreateScreenPass(device, swapChain);
            swapChain->AcquireNextImage();
        }

        // Record render pass
        ctx->BeginRenderPass(*screenPass,
            { CClearValue(0.2f, 0.3f, 0.4f, 0.0f), CClearValue(1.0f, 0) });
        ctx->BindPipeline(*pso);
        ctx->BindBuffer(*ubo, 0, 16, 0, 1, 0);
        ctx->BindVertexBuffer(0, *vbo, 0);
        ctx->BindSampler(*sampler, 1, 0, 0);
        ctx->BindImageView(*checkerView, 1, 1, 0);
        ctx->Draw(3, 1, 0, 0);

        ImGui::Render();
        CRHIImGuiBackend::RenderDrawData(ImGui::GetDrawData(), ctx);

        ctx->EndRenderPass();

        // Present
        CSwapChainPresentInfo info;
        info.SrcImage = nullptr;
        swapChain->Present(info);
    }

    physicsThread.join();

    // Sync & exit
    ctx->Flush(true);
    CRHIImGuiBackend::Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDL_Delay(1000);
    return 0;
}