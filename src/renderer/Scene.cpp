#include "renderer/Scene.h"
#include "core/Logger.h"

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// --- Lifecycle ---

void Scene::init()
{
	LOG_DEBUG << "Initializing Scene";

	initImGui();
	initFrameBuffers();
	initShaders();
	initSceneObjects();
	initIBL();
	initLights();
	updateSSAOBlurWeights();
	updateSSDOBlurWeights();
	updateSSDOIndirectBlurWeights();
}

Scene::~Scene()
{
	if (m_quadVAO != 0) {
		glDeleteVertexArrays(1, &m_quadVAO);
		glDeleteBuffers(1, &m_quadVBO);
	}
	if (m_samplerRepeat != 0) {
		glDeleteSamplers(1, &m_samplerRepeat);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Scene::update(float deltaTime)
{
}

void Scene::drawScene()
{
	geometryPass();

	if (m_momentShadow.enable) {
		shadowPass();
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (m_ssao.enable) {
		ssaoPass();
	}

	if (m_ssdo.enable) {
		ssdoDirectPass();
		ssdoIndirectPass();
	}

	if (m_ssr.enable) {
		ssrPass();
	}

	lightingPass();

	m_gBuffer->persistDepthInfo();

	if (!m_enableDebug) {
		drawSkydome();
	}

	drawMenu();
}

// --- Initialization ---

void Scene::initImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
}

void Scene::initFrameBuffers()
{
	m_gBuffer = std::make_unique<FrameBuffer>();
	m_gBuffer->init(m_width, m_height, 4, true);

	m_rawSSAOBuffer = std::make_unique<FrameBuffer>();
	m_rawSSAOBuffer->init(m_width, m_height, 1, false);

	m_blurSSAOBuffer = std::make_unique<FrameBuffer>();
	m_blurSSAOBuffer->init(m_width, m_height, 1, false);

	m_shadowBuffer = std::make_unique<FrameBuffer>();
	m_shadowBuffer->initShadowFBO(2048, 2048);

	m_ssdoDirectFBO = std::make_unique<FrameBuffer>();
	m_ssdoDirectFBO->init(m_width, m_height, 3, false);

	m_ssdoDirectBlurFBO = std::make_unique<FrameBuffer>();
	m_ssdoDirectBlurFBO->init(m_width, m_height, 2, false);

	m_ssdoIndirectFBO = std::make_unique<FrameBuffer>();
	m_ssdoIndirectFBO->init(m_width, m_height, 1, false);

	m_ssdoIndirectBlurFBO = std::make_unique<FrameBuffer>();
	m_ssdoIndirectBlurFBO->init(m_width, m_height, 1, false);

	m_ssrFBO = std::make_unique<FrameBuffer>();
	m_ssrFBO->init(m_width, m_height, 1, false);
}

void Scene::initShaders()
{
	m_gBufferShader = std::make_unique<Shader>("gbuffer.vert", "gbuffer.frag");
	m_ssaoShader = std::make_unique<Shader>("postprocess.vert", "rawssao.frag");
	m_ssaoBlurShader = std::make_unique<Shader>("postprocess.vert", "blurssao.frag");
	m_shadowShader = std::make_unique<Shader>("shadow.vert", "shadow.frag");
	m_ssdoDirectShader = std::make_unique<Shader>("postprocess.vert", "rawdirectssdo.frag");
	m_ssdoIndirectShader = std::make_unique<Shader>("postprocess.vert", "rawindirectssdo.frag");
	m_ssdoBlurShader = std::make_unique<Shader>("postprocess.vert", "blurssdo.frag");

	m_lightingShader = std::make_unique<Shader>("postprocess.vert", "deferred_lighting.frag");
	m_debugShader = std::make_unique<Shader>("postprocess.vert", "deferred_lighting_debug.frag");
	m_ssrShader = std::make_unique<Shader>("postprocess.vert", "ssr.frag");

	m_hmGenerator.generatePairs(m_ibl.numSamples);
	m_hmGenerator.initializeUBO();
	m_hmGenerator.registerShader(m_lightingShader->id());
	m_hmGenerator.registerShader(m_debugShader->id());
}

void Scene::initSceneObjects()
{
	m_models.push_back(std::make_unique<Model>(
		"san_miguel/gltf/san_miguel.gltf", glm::vec3(0.0f), 0.0f, 1.0f));

	float scale = m_models.back()->scale();
	m_sceneMin = m_models.back()->sceneMin() * scale;
	m_sceneMax = m_models.back()->sceneMax() * scale;

	m_currentModel = m_models[m_currentModelIdx].get();
	m_modelNames.reserve(m_models.size());
	for (const auto& model : m_models) {
		m_modelNames.push_back(model->modelName());
	}

	m_skydomeShader = std::make_unique<Shader>("skydome.vert", "skydome.frag");
	m_skyDome = std::make_unique<SkyDome>();

	glGenSamplers(1, &m_samplerRepeat);
	glSamplerParameteri(m_samplerRepeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(m_samplerRepeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameteri(m_samplerRepeat, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(m_samplerRepeat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Scene::initIBL()
{
	for (const auto& name : m_envMapNames) {
		m_shIrradiance.computeFromHDR(name);
	}
}

void Scene::initLights()
{
	m_directionalLight = std::make_unique<DirectionalLight>(
		glm::vec3(-2.0f, -5.0f, -1.0f),
		glm::vec3(1.0f),
		0.0f);

	setDirectionalLightFromSun();
	m_currentLight = m_directionalLight.get();
	m_shadowBlur = std::make_unique<ShadowMapBlur>();
}

void Scene::setDirectionalLightFromSun()
{
	glm::vec3 sunDir = m_shIrradiance.getL1SunDirection();
	glm::vec3 sunRadiance = m_shIrradiance.getSunColor();
	float sunIntensity = 0.2126f * sunRadiance.r + 0.7152f * sunRadiance.g + 0.0722f * sunRadiance.b;
	glm::vec3 sunColor = sunRadiance / sunIntensity;

	m_directionalLight->setDirection(-sunDir);
	m_directionalLight->color() = sunColor;
	m_directionalLight->intensity() = sunIntensity;
}

// --- Rendering passes ---

void Scene::ssrPass()
{
	m_ssrFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssrShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssrShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssrShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_gBuffer->getTextureID(2));
	m_ssrShader->setInt("gAlbedo", 2);
	glBindTextureUnit(3, m_gBuffer->getTextureID(3));
	m_ssrShader->setInt("gMetalRough", 3);

	float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
	glm::mat4 projection = glm::perspective(glm::radians(camera.getZoom()), aspect, 0.5f, 500.0f);
	glm::mat4 view = camera.getViewMatrix();

	m_ssrShader->setMat4("projection", projection);
	m_ssrShader->setMat4("view", view);
	m_ssrShader->setVec3("viewPos", camera.getPosition());
	m_ssrShader->setInt("maxSteps", m_ssr.maxSteps);
	m_ssrShader->setFloat("stepSize", m_ssr.stepSize);
	m_ssrShader->setFloat("thickness", m_ssr.thickness);
	m_ssrShader->setInt("binarySearchSteps", m_ssr.binarySearchSteps);

	renderQuad();
	m_ssrFBO->unbind();
}

void Scene::shadowPass()
{
	m_shadowBuffer->bind();
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_shadowShader->use();
	m_isShadowPass = true;
	drawModels(m_shadowShader.get());
	m_isShadowPass = false;

	m_shadowBuffer->unbind();
	m_shadowBlur->performBlur(m_shadowBuffer.get());
}

void Scene::geometryPass()
{
	m_gBuffer->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_gBufferShader->use();
	m_gBufferShader->setBool("disableNormalMaps", m_disableNormalMaps);
	drawModels(m_gBufferShader.get());

	m_gBuffer->unbind();
	glDisable(GL_CULL_FACE);
}

void Scene::lightingPass()
{
	Shader* shader = m_enableDebug ? m_debugShader.get() : m_lightingShader.get();

	shader->use();
	m_hmGenerator.bindUBOToShader();

	glViewport(0, 0, m_width, m_height);
	glDisable(GL_DEPTH_TEST);

	bindGBufferTextures(shader);

	shader->setVec3("viewPos", camera.getPosition());
	shader->setFloat("exposure", m_sceneExposure);
	shader->setBool("light.enable", m_enableDirectionalLight);

	if (m_currentLight) {
		m_currentLight->setLightingUniforms(shader);
		setShadowUniforms(shader);
	}

	setIBLUniforms(shader);
	setSSAOUniforms(shader);
	setSSDOUniforms(shader);
	setSSRUniforms(shader);

	shader->setInt("debugMode", m_debugMode);
	renderQuad();

	glEnable(GL_DEPTH_TEST);
}

// --- SSAO ---

void Scene::ssaoPass()
{
	ssaoRawPass();
	ssaoBlurPass();
}

void Scene::ssaoRawPass()
{
	m_rawSSAOBuffer->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssaoShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssaoShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssaoShader->setInt("gNormal", 1);

	m_ssaoShader->setFloat("R", m_ssao.radius);
	m_ssaoShader->setFloat("bias", m_ssao.bias);
	m_ssaoShader->setInt("n", m_ssao.numSamples);
	m_ssaoShader->setFloat("s", m_ssao.scale);
	m_ssaoShader->setFloat("k", m_ssao.contrast);

	renderQuad();
	m_rawSSAOBuffer->unbind();
}

void Scene::ssaoBlurPass()
{
	m_blurSSAOBuffer->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssaoBlurShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssaoBlurShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssaoBlurShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_rawSSAOBuffer->getTextureID(0));
	m_ssaoBlurShader->setInt("ssaoMap", 2);

	m_ssaoBlurShader->setInt("radius", m_ssao.blurRadius);
	m_ssaoBlurShader->setFloat("s", m_ssao.blurVariance);
	m_ssaoBlurShader->setFloatArray("gaussianWeights", m_ssao.blurRadius + 1, m_ssaoBlurWeights.data());

	renderQuad();
	m_blurSSAOBuffer->unbind();
}

void Scene::updateSSAOBlurWeights()
{
	m_ssaoBlurWeights = computeGaussianWeights(m_ssao.blurRadius);
}

// --- SSDO ---

void Scene::ssdoDirectPass()
{
	m_ssdoDirectFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssdoDirectShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssdoDirectShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssdoDirectShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_gBuffer->getTextureID(2));
	m_ssdoDirectShader->setInt("gAlbedo", 2);
	glBindTextureUnit(3, m_shIrradiance.getCurrentEnvMapTexId());
	m_ssdoDirectShader->setInt("envMap", 3);

	setPVWUniforms(m_ssdoDirectShader.get());
	m_ssdoDirectShader->setVec3("viewPos", camera.getPosition());
	m_ssdoDirectShader->setFloat("R", m_ssdo.radius);
	m_ssdoDirectShader->setInt("n", m_ssdo.numSamples);
	m_ssdoDirectShader->setFloat("contrast", m_ssdo.contrast);
	m_ssdoDirectShader->setFloat("lod", m_ssdo.lod);

	m_shIrradiance.uploadToShader(m_ssdoDirectShader.get());
	renderQuad();

	m_ssdoDirectFBO->unbind();
	blurSSDODirect();
}

void Scene::blurSSDODirect()
{
	m_ssdoDirectBlurFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssdoBlurShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssdoBlurShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssdoBlurShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_ssdoDirectFBO->getTextureID(0));
	m_ssdoBlurShader->setInt("inputMap0", 2);
	glBindTextureUnit(3, m_ssdoDirectFBO->getTextureID(1));
	m_ssdoBlurShader->setInt("inputMap1", 3);

	m_ssdoBlurShader->setInt("radius", m_ssdo.blurRadius);
	m_ssdoBlurShader->setInt("blurMode", 1);
	m_ssdoBlurShader->setFloat("s", m_ssdo.blurVariance);
	m_ssdoBlurShader->setFloatArray("gaussianWeights", m_ssdo.blurRadius + 1, m_ssdoBlurWeights.data());

	renderQuad();
	m_ssdoDirectBlurFBO->unbind();
}

void Scene::updateSSDOBlurWeights()
{
	m_ssdoBlurWeights = computeGaussianWeights(m_ssdo.blurRadius);
}

void Scene::ssdoIndirectPass()
{
	m_ssdoIndirectFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssdoIndirectShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssdoIndirectShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssdoIndirectShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_gBuffer->getTextureID(2));
	m_ssdoIndirectShader->setInt("gAlbedo", 2);
	glBindTextureUnit(3, m_ssdoDirectFBO->getTextureID(1));
	m_ssdoIndirectShader->setInt("ssdoRadMap", 3);

	setPVWUniforms(m_ssdoIndirectShader.get());
	m_ssdoIndirectShader->setVec3("viewPos", camera.getPosition());
	m_ssdoIndirectShader->setFloat("R", m_ssdo.indirectRadius);
	m_ssdoIndirectShader->setFloat("dClamp", m_ssdo.distanceClamp);
	m_ssdoIndirectShader->setFloat("strength", m_ssdo.strength);
	m_ssdoIndirectShader->setInt("n", m_ssdo.numSamples);

	renderQuad();
	m_ssdoIndirectFBO->unbind();

	blurSSDOIndirect();
}

void Scene::blurSSDOIndirect()
{
	m_ssdoIndirectBlurFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_ssdoBlurShader->use();

	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	m_ssdoBlurShader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	m_ssdoBlurShader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_ssdoIndirectFBO->getTextureID(0));
	m_ssdoBlurShader->setInt("inputMap0", 2);

	m_ssdoBlurShader->setInt("radius", m_ssdo.indirectBlurRadius);
	m_ssdoBlurShader->setFloat("s", m_ssdo.blurVariance);
	m_ssdoBlurShader->setFloatArray("gaussianWeights",
		m_ssdo.indirectBlurRadius + 1, m_ssdoIndirectBlurWeights.data());
	m_ssdoBlurShader->setInt("blurMode", 0);

	renderQuad();
	m_ssdoIndirectBlurFBO->unbind();
}

void Scene::updateSSDOIndirectBlurWeights()
{
	m_ssdoIndirectBlurWeights = computeGaussianWeights(m_ssdo.indirectBlurRadius);
}

// --- Shader helpers ---

void Scene::bindGBufferTextures(Shader* shader)
{
	glBindTextureUnit(0, m_gBuffer->getTextureID(0));
	shader->setInt("gPosition", 0);
	glBindTextureUnit(1, m_gBuffer->getTextureID(1));
	shader->setInt("gNormal", 1);
	glBindTextureUnit(2, m_gBuffer->getTextureID(2));
	shader->setInt("gAlbedo", 2);
	glBindTextureUnit(3, m_gBuffer->getTextureID(3));
	shader->setInt("gMetalRough", 3);
}

void Scene::setShadowUniforms(Shader* shader)
{
	shader->setBool("momentShadow.enable", m_momentShadow.enable);
	if (!m_currentLight) return;

	m_currentLight->setShadowUniforms(shader, m_sceneMin, m_sceneMax);

	shader->setFloat("momentShadow.normalBias", m_momentShadow.normalBias);
	shader->setFloat("momentShadow.depthBias", m_momentShadow.depthBias);
	shader->setFloat("momentShadow.scale", m_momentShadow.scale);
	shader->setFloat("momentShadow.visScale", m_momentShadow.visibilityScale);
	shader->setFloat("momentShadow.bias", m_momentShadow.bias);
	shader->setFloat("momentShadow.bleedReduction", m_momentShadow.bleedReduction);
	shader->setBool("momentShadow.useDirectScaledMoments", m_momentShadow.useDirectScaledMoments);

	glBindTextureUnit(4, m_shadowBuffer->getTextureID(0));
	shader->setInt("shadowMap", 4);
	glBindTextureUnit(5, m_shadowBlur->getBlurredShadowMapTexID());
	shader->setInt("blurredShadowMap", 5);
}

void Scene::setIBLUniforms(Shader* shader)
{
	shader->setBool("ibl.enable", m_ibl.enable);
	shader->setBool("ibl.diffuseOnly", m_ibl.diffuseOnly);
	shader->setInt("ibl.N", m_ibl.numSamples);
	shader->setFloat("ibl.intensity", m_ibl.intensity);

	m_shIrradiance.uploadToShader(shader);

	glBindTextureUnit(6, m_shIrradiance.getCurrentEnvMapTexId());
	glBindSampler(6, m_samplerRepeat);
	shader->setInt("envMap", 6);
}

void Scene::setSSAOUniforms(Shader* shader)
{
	shader->setBool("ssao.enable", m_ssao.enable);
	shader->setBool("ssao.enableBlur", m_ssao.enableBlur);

	glBindTextureUnit(7, m_rawSSAOBuffer->getTextureID(0));
	shader->setInt("ssaoMap", 7);
	glBindTextureUnit(8, m_blurSSAOBuffer->getTextureID(0));
	shader->setInt("blurredssaoMap", 8);
}

void Scene::setSSDOUniforms(Shader* shader)
{
	shader->setBool("ssdo.enable", m_ssdo.enable);
	shader->setBool("ssdo.enableIndirect", m_ssdo.enableIndirect);
	shader->setInt("ssdo.doMethod", m_ssdo.doMethod);

	glBindTextureUnit(9, m_ssdoDirectFBO->getTextureID(0));
	shader->setInt("rawSSDOVisibilityMap", 9);
	glBindTextureUnit(10, m_ssdoDirectBlurFBO->getTextureID(0));
	shader->setInt("blurSSDOVisibilityMap", 10);
	glBindTextureUnit(11, m_ssdoDirectFBO->getTextureID(1));
	shader->setInt("rawSSDORadianceMap", 11);
	glBindTextureUnit(12, m_ssdoDirectBlurFBO->getTextureID(1));
	shader->setInt("blurSSDORadianceMap", 12);
	glBindTextureUnit(13, m_ssdoIndirectFBO->getTextureID(0));
	shader->setInt("indirectSSDOMap", 13);
	glBindTextureUnit(14, m_ssdoIndirectBlurFBO->getTextureID(0));
	shader->setInt("blurindirectSSDOMap", 14);
}

void Scene::setSSRUniforms(Shader* shader)
{
	shader->setBool("ssr.enable", m_ssr.enable);
	glBindTextureUnit(15, m_ssrFBO->getTextureID(0));
	shader->setInt("ssrMap", 15);
}

void Scene::setPVWUniforms(Shader* shader)
{
	shader->use();

	if (m_isShadowPass && m_enableDirectionalLight) {
		glm::mat4 view = m_currentLight->getViewMatrix(m_sceneMin, m_sceneMax);
		glm::mat4 projection = m_currentLight->getProjectionMatrix(m_sceneMin, m_sceneMax);
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);
	}
	else {
		float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
		glm::mat4 projection = glm::perspective(glm::radians(camera.getZoom()), aspect, 0.5f, 500.0f);
		glm::mat4 view = camera.getViewMatrix();
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);

		if (m_enableDirectionalLight) {
			glm::mat4 lightView = m_currentLight->getViewMatrix(m_sceneMin, m_sceneMax);
			glm::mat4 lightProjection = m_currentLight->getProjectionMatrix(m_sceneMin, m_sceneMax);
			shader->setMat4("lightViewMatrix", lightView);
			shader->setMat4("lightprojection", lightProjection);
		}
	}
}

// --- Drawing helpers ---

void Scene::renderQuad()
{
	if (m_quadVAO == 0) {
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};

		glGenVertexArrays(1, &m_quadVAO);
		glGenBuffers(1, &m_quadVBO);
		glBindVertexArray(m_quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			reinterpret_cast<void*>(0));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			reinterpret_cast<void*>(3 * sizeof(float)));
	}

	glBindVertexArray(m_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void Scene::drawModels(Shader* shader)
{
	setPVWUniforms(shader);
	for (auto& model : m_models) {
		model->Draw(shader);
	}
}

void Scene::drawSkydome()
{
	m_skydomeShader->use();
	float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
	glm::mat4 projection = glm::perspective(glm::radians(camera.getZoom()), aspect, 0.1f, 500.0f);
	glm::mat4 view = glm::mat4(glm::mat3(camera.getViewMatrix()));

	m_skydomeShader->setMat4("projection", projection);
	m_skydomeShader->setMat4("view", view);
	m_skydomeShader->setFloat("exposure", m_skyglobeExposure);
	m_skyDome->Draw(m_skydomeShader.get(), m_shIrradiance.getCurrentEnvMapTexId());
}

// --- Utility ---

std::vector<float> Scene::computeGaussianWeights(int radius)
{
	std::vector<float> weights;
	float s = static_cast<float>(radius) / 2.0f;
	float sum = 0.0f;

	for (int i = 0; i <= radius; i++) {
		float ex = -0.5f * pow(static_cast<float>(i) / s, 2.0f);
		float kw = exp(ex);
		weights.push_back(kw);
		sum += kw;
	}

	sum = 2.0f * sum - weights[0];
	for (auto& w : weights) {
		w /= sum;
	}

	return weights;
}

// --- UI ---

void Scene::drawMenu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Scene Controls");

	ImGui::Text("Mode: %s", camera.getModeName());

	if (ImGui::Button("Log Camera Information")) {
		camera.logPosition();
	}

	drawDebugSection();
	ImGui::Separator();
	drawLightingSection();
	drawShadowSection();
	drawSSRSection();
	drawSSAOSection();
	drawSSDOSection();
	ImGui::Separator();
	drawIBLSection();
	ImGui::Separator();
	drawModelSection();

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Scene::drawDebugSection()
{
	ImGui::Text("G-Buffer Debug Modes:");
	ImGui::Checkbox("Enable Debug", &m_enableDebug);
	ImGui::RadioButton("Position", &m_debugMode, 0);
	ImGui::RadioButton("Normal", &m_debugMode, 1);
	ImGui::RadioButton("Depth", &m_debugMode, 2);
	ImGui::RadioButton("Albedo", &m_debugMode, 3);
	ImGui::RadioButton("Metallic", &m_debugMode, 4);
	ImGui::RadioButton("Roughness", &m_debugMode, 5);

	if (m_momentShadow.enable) {
		ImGui::RadioButton("Shadow Map", &m_debugMode, 6);
		ImGui::RadioButton("Blurred Shadow Map", &m_debugMode, 7);
	}

	if (m_ssao.enable) {
		ImGui::RadioButton("Raw SSAO", &m_debugMode, 9);
		ImGui::RadioButton("Blurred SSAO", &m_debugMode, 10);
	}

	if (m_ssdo.enable) {
		ImGui::RadioButton("Raw SSDO Visibility", &m_debugMode, 11);
		ImGui::RadioButton("Blurred SSDO Visibility", &m_debugMode, 12);
		ImGui::RadioButton("Raw SSDO Radiance", &m_debugMode, 13);
		ImGui::RadioButton("Blurred SSDO Radiance", &m_debugMode, 14);
		ImGui::RadioButton("Raw SSDO Indirect", &m_debugMode, 15);
		ImGui::RadioButton("Blurred SSDO Indirect", &m_debugMode, 16);
	}

	if (m_ssr.enable) {
		ImGui::RadioButton("SSR Color", &m_debugMode, 17);
		ImGui::RadioButton("SSR Confidence", &m_debugMode, 18);
		ImGui::RadioButton("SSR Final", &m_debugMode, 19);
	}
}

void Scene::drawLightingSection()
{
	ImGui::Text("Lighting Settings:");
	ImGui::Separator();

	if (ImGui::Checkbox("Enable Directional Light", &m_enableDirectionalLight)) {
		m_currentLight = m_enableDirectionalLight ? m_directionalLight.get() : nullptr;
	}

	if (!m_enableDirectionalLight) return;

	if (ImGui::Checkbox("Manual Light Control", &m_manualLightControl)) {
		if (!m_manualLightControl) {
			setDirectionalLightFromSun();
		}
	}
	ImGui::Separator();

	if (m_manualLightControl) {
		ImGui::SliderFloat3("Direction", glm::value_ptr(m_directionalLight->direction()), -1.0f, 1.0f);
		ImGui::ColorEdit3("Color", glm::value_ptr(m_directionalLight->color()));
		ImGui::SliderFloat("Intensity", &m_directionalLight->intensity(), 0.0f, 30.0f);
	}
	else {
		glm::vec3 dir = -m_directionalLight->direction();
		ImGui::Text("Direction : %.3f, %.3f, %.3f", dir.x, dir.y, dir.z);
		const glm::vec3& col = m_directionalLight->color();
		ImGui::Text("Color     : %.3f, %.3f, %.3f", col.r, col.g, col.b);
		ImGui::Text("Intensity : %.3f", m_directionalLight->intensity());
	}
}

void Scene::drawShadowSection()
{
	ImGui::Separator();
	ImGui::Checkbox("Enable Shadows", &m_momentShadow.enable);

	if (m_momentShadow.enable) {
		if (ImGui::SliderInt("Shadow map Blur Radius", &m_momentShadow.blurRadius, 1, 20)) {
			m_shadowBlur->updateBlur(m_momentShadow.blurRadius);
		}

		ImGui::Text("Shadow Settings:");
		ImGui::SliderFloat("Normal Bias", &m_momentShadow.normalBias, 0.0f, 0.1f);
		ImGui::SliderFloat("Depth Bias", &m_momentShadow.depthBias, 0.0f, 0.1f);

		ImGui::Text("Moment Shadow Settings:");
		ImGui::SliderFloat("Alpha", &m_momentShadow.bias, 1.0f, 10.0f);
		ImGui::SliderFloat("Moments Scale", &m_momentShadow.scale, 0.95f, 1.0f);
		ImGui::SliderFloat("Visibility Scale", &m_momentShadow.visibilityScale, 1.0f, 2.0f);
		ImGui::SliderFloat("Light Bleed Reduction Factor", &m_momentShadow.bleedReduction, 0.0f, 1.0f);
		ImGui::Checkbox("Use direct scaled moments", &m_momentShadow.useDirectScaledMoments);
	}
}

void Scene::drawSSRSection()
{
	ImGui::Text("SSR Settings:");
	ImGui::Checkbox("Enable SSR", &m_ssr.enable);

	if (m_ssr.enable) {
		ImGui::SliderInt("Max Steps", &m_ssr.maxSteps, 16, 256);
		ImGui::SliderFloat("Step Size", &m_ssr.stepSize, 0.01f, 1.0f);
		ImGui::SliderFloat("Thickness", &m_ssr.thickness, 0.001f, 1.0f);
		ImGui::SliderInt("Binary Search Steps", &m_ssr.binarySearchSteps, 0, 16);
	}
}

void Scene::drawSSAOSection()
{
	ImGui::Text("SSAO Settings:");
	ImGui::Checkbox("Enable SSAO", &m_ssao.enable);

	if (m_ssao.enable) {
		ImGui::Checkbox("Enable SSAO Blur", &m_ssao.enableBlur);
		ImGui::SliderFloat("SSAO Range", &m_ssao.radius, 0.001f, 0.5f);
		ImGui::SliderFloat("SSAO bias", &m_ssao.bias, 0.001f, 0.005f);
		ImGui::SliderFloat("SSAO scale", &m_ssao.scale, 1.0f, 5.0f);
		ImGui::SliderFloat("SSAO contrast", &m_ssao.contrast, 1.0f, 5.0f);
		ImGui::SliderInt("Num of Sample Points", &m_ssao.numSamples, 10, 30);
		ImGui::SliderFloat("SSAO Blur Sigma", &m_ssao.blurVariance, 0.01f, 0.05f);

		if (ImGui::SliderInt("SSAO Map Blur Radius", &m_ssao.blurRadius, 1, 10)) {
			updateSSAOBlurWeights();
		}
	}
}

void Scene::drawSSDOSection()
{
	ImGui::Text("SSDO Settings:");
	ImGui::Checkbox("Enabled SSDO", &m_ssdo.enable);

	if (m_ssdo.enable) {
		ImGui::Text("DO Parameters");
		ImGui::Text("DO Method");
		ImGui::SameLine();
		ImGui::RadioButton("Visibility", &m_ssdo.doMethod, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Radiance", &m_ssdo.doMethod, 1);

		ImGui::SliderFloat("SSDO Range", &m_ssdo.radius, 0.001f, 0.5f);
		ImGui::SliderFloat("SSDO Env Map LOD", &m_ssdo.lod, 0.0f, 15.0f);
		ImGui::SliderFloat("SSDO Contrast", &m_ssdo.contrast, 1.0f, 10.0f);
		ImGui::SliderInt("Num of Sample Points", &m_ssdo.numSamples, 10, 100);

		if (ImGui::SliderInt("Direct SSDO Map Blur Radius", &m_ssdo.blurRadius, 1, 10)) {
			updateSSDOBlurWeights();
		}

		ImGui::Checkbox("Enabled SSDO Indirect", &m_ssdo.enableIndirect);

		if (m_ssdo.enableIndirect) {
			ImGui::Text("GI Parameters");
			ImGui::SliderFloat("SSDO GI d clamp", &m_ssdo.distanceClamp, 0.1f, 1.0f);
			ImGui::SliderFloat("SSDO GI strength", &m_ssdo.strength, 1.0f, 50.0f);
			ImGui::SliderFloat("SSDO GI Range", &m_ssdo.indirectRadius, 0.1f, 5.0f);

			if (ImGui::SliderInt("Indirect SDDO Map Blur Radius", &m_ssdo.indirectBlurRadius, 1, 10)) {
				updateSSDOIndirectBlurWeights();
			}
		}
	}
}

void Scene::drawIBLSection()
{
	ImGui::Text("IBL Settings:");
	ImGui::Checkbox("Enabled IBL", &m_ibl.enable);

	if (m_ibl.enable) {
		ImGui::SliderFloat("Scene Exposure", &m_sceneExposure, 0.0f, 20.0f);
		ImGui::SliderFloat("Skydome Exposure", &m_skyglobeExposure, 0.0f, 5.0f);
		ImGui::Checkbox("Vertex Normals Only", &m_disableNormalMaps);
		ImGui::SliderFloat("IBL Intensity", &m_ibl.intensity, 0.0f, 1.0f);

		if (ImGui::SliderInt("Num Samples", &m_ibl.numSamples, 1, 1024)) {
			m_hmGenerator.updateUBO(m_ibl.numSamples);
		}

		if (ImGui::Combo("Env Map", &m_currEnvMapIdx,
			[](void* data, int idx, const char** out_text) {
				*out_text = static_cast<std::vector<std::string>*>(data)->at(idx).c_str();
				return true;
			}, &m_envMapNames, static_cast<int>(m_envMapNames.size()))) {

			m_shIrradiance.currMapIdx = m_currEnvMapIdx;
			setDirectionalLightFromSun();
		}
	}
}

void Scene::drawModelSection()
{
	ImGui::Text("Model Settings:");
	ImGui::SliderFloat("Model Scale", &m_currentModel->scale(), 0.1f, 30.0f);
	ImGui::SliderFloat("Model Rotate", &m_currentModel->rotateY(), -180.0f, 180.0f);
	ImGui::SliderFloat3("Model Position", glm::value_ptr(m_currentModel->position()), -200.0f, 200.0f);

	if (ImGui::Combo("Current Model", &m_currentModelIdx,
		[](void* data, int idx, const char** out_text) {
			*out_text = static_cast<std::vector<std::string>*>(data)->at(idx).c_str();
			return true;
		}, &m_modelNames, static_cast<int>(m_modelNames.size()))) {

		m_currentModel = m_models[m_currentModelIdx].get();
	}
}
