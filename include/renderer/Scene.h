#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#define GLFW_INCLUDE_NONE
#include <glfw/glfw3.h>
#include <glad/glad.h>

#include "camera/Camera.h"
#include "renderer/Shader.h"
#include "geometry/Model.h"
#include "lighting/Light.h"
#include "lighting/SkyDome.h"
#include "renderer/FrameBuffer.h"
#include "lighting/SHIrradiance.h"
#include "renderer/ShadowMapBlur.h"
#include "lighting/DirectionalLight.h"
#include "lighting/HammersleyRandomGenerator.h"
#include "core/RenderConfig.h"

class Scene {
public:
	GLFWwindow* window = nullptr;
	Camera camera{glm::vec3(13.3f, 1.8f, 10.5f)};

	void init();
	void update(float deltaTime);
	void drawScene();

	~Scene();

private:
	int m_width = 2560;
	int m_height = 1440;

	// --- Initialization ---
	void initImGui();
	void initFrameBuffers();
	void initShaders();
	void initSceneObjects();
	void initIBL();
	void initLights();

	// --- Rendering passes ---
	void geometryPass();
	void lightingPass();
	void shadowPass();
	void ssrPass();

	// SSAO
	void ssaoPass();
	void ssaoRawPass();
	void ssaoBlurPass();
	void updateSSAOBlurWeights();

	// SSDO
	void ssdoDirectPass();
	void blurSSDODirect();
	void updateSSDOBlurWeights();
	void ssdoIndirectPass();
	void blurSSDOIndirect();
	void updateSSDOIndirectBlurWeights();

	// --- Shader helpers ---
	void bindGBufferTextures(Shader* shader);
	void setIBLUniforms(Shader* shader);
	void setSSAOUniforms(Shader* shader);
	void setSSDOUniforms(Shader* shader);
	void setPVWUniforms(Shader* shader);
	void setShadowUniforms(Shader* shader);
	void setSSRUniforms(Shader* shader);
	void setDirectionalLightFromSun();

	// --- Drawing helpers ---
	void renderQuad();
	void drawModels(Shader* shader);
	void drawSkydome();
	void drawMenu();

	// --- UI sections ---
	void drawDebugSection();
	void drawLightingSection();
	void drawShadowSection();
	void drawSSRSection();
	void drawSSAOSection();
	void drawSSDOSection();
	void drawIBLSection();
	void drawModelSection();

	// --- Utility ---
	static std::vector<float> computeGaussianWeights(int radius);

	// --- Framebuffers ---
	std::unique_ptr<FrameBuffer> m_gBuffer;
	std::unique_ptr<FrameBuffer> m_shadowBuffer;
	std::unique_ptr<FrameBuffer> m_rawSSAOBuffer, m_blurSSAOBuffer;
	std::unique_ptr<FrameBuffer> m_ssdoDirectFBO, m_ssdoDirectBlurFBO;
	std::unique_ptr<FrameBuffer> m_ssdoIndirectFBO, m_ssdoIndirectBlurFBO;
	std::unique_ptr<FrameBuffer> m_ssrFBO;

	// --- Shaders ---
	std::unique_ptr<Shader> m_gBufferShader, m_lightingShader, m_debugShader;
	std::unique_ptr<Shader> m_shadowShader, m_skydomeShader;
	std::unique_ptr<Shader> m_ssaoShader, m_ssaoBlurShader;
	std::unique_ptr<Shader> m_ssdoDirectShader, m_ssdoBlurShader, m_ssdoIndirectShader;
	std::unique_ptr<Shader> m_ssrShader;

	std::unique_ptr<ShadowMapBlur> m_shadowBlur;

	// --- Quad rendering ---
	GLuint m_quadVAO = 0, m_quadVBO = 0;
	GLuint m_samplerRepeat = 0;

	// --- Scene objects ---
	std::vector<std::unique_ptr<Model>> m_models;
	glm::vec3 m_sceneMin{-1.5f, -0.05f, -1.5f};
	glm::vec3 m_sceneMax{1.5f, 3.0f, 1.5f};
	std::unique_ptr<SkyDome> m_skyDome;
	std::vector<std::string> m_modelNames;
	Model* m_currentModel = nullptr;
	int m_currentModelIdx = 0;

	// --- Debug ---
	int m_debugMode = 0;
	bool m_enableDebug = false;
	bool m_disableNormalMaps = true;

	// --- Lighting ---
	Light* m_currentLight = nullptr;
	bool m_isShadowPass = false;
	MomentShadow m_momentShadow;
	bool m_manualLightControl = false;
	bool m_enableDirectionalLight = false;
	std::unique_ptr<DirectionalLight> m_directionalLight;

	// --- SSR ---
	SSRConfig m_ssr;

	// --- SSAO ---
	SSAOConfig m_ssao;
	std::vector<float> m_ssaoBlurWeights;

	// --- SSDO ---
	SSDOConfig m_ssdo;
	std::vector<float> m_ssdoBlurWeights;
	std::vector<float> m_ssdoIndirectBlurWeights;

	// --- IBL ---
	IBLConfig m_ibl;
	int m_currEnvMapIdx = 0;
	float m_sceneExposure = 0.7f;
	float m_skyglobeExposure = 1.0f;

	SHIrradiance m_shIrradiance;
	HammersleyRandomGenerator m_hmGenerator;

	std::vector<std::string> m_envMapNames{
		"kloppenheim_06_puresky_4k.exr",
		"cedar_bridge_sunset_1_4k.exr",
	};
};
