// Player headtracking, suavecito y que sigue a la camara
// mas como starfield que como oblivion que lockea un NPC.
// ¡Viva el Club Deportivo Palestino y Viva Palestina libre!
// Kechu.

#include "PCH.hpp"
#include <SimpleIni.h>
#include <Windows.h>
#include <atomic>
#include <string>

#include "RE/S/SandmanKill_Event.hpp" // Sandman

using namespace RE;

namespace
{
	constexpr float PI = 3.14159265358979323846f; // soto matias

	// Settings
	std::atomic<float> g_smoothSpeed{ 10.0f };
	std::atomic<float> g_giveUpYawDeg{ 90.0f };
	std::atomic<float> g_headMaxYawDeg{ 55.0f };
	std::atomic<float> g_spine2MaxYawDeg{ 15.0f };
	std::atomic<float> g_maxPitchUpDeg{ 35.0f };
	std::atomic<float> g_maxPitchDownDeg{ 30.0f };

	std::atomic<int> g_toggleKeyVK{ 0 };				 // sin bind
	std::atomic<bool> g_disableWithWeaponDrawn{ false }; // false = comportamiento gunState::kRelaxed-only
	std::atomic<bool> g_trackingEnabled{ true };


	// idle
	std::atomic<float> g_idleTimeoutSeconds{ 5.0f };
	float g_idleTimer = 0.0f;
	NiPoint3 g_lastPlayerPosition{ 0.0f, 0.0f, 0.0f };
	float g_lastCamYaw = 0.0f;
	float g_lastCamPitch = 0.0f;

	// Tolerancias
	constexpr float IDLE_POSITION_EPSILON = 1.0f;
	constexpr float IDLE_CAM_EPSILON = 0.01f;

	// Neck ratio
	std::atomic<float> g_neckRatio{ 0.6f };

	// Look at camera
	std::atomic<bool> g_lookAtCamera{ true };
	float g_lookAtCamWeight = 0.0f; // 0 = mira, 1 = camara
	std::atomic<float> g_lookAtAngleDeg{ 45.0f }; // cono de enfrente donde te mira
	std::atomic<float> g_lookAtDelay{ 0.4f };     // seg en el centro antes de mirarte
	std::atomic<float> g_lookAtSpeed{ 2.0f };     // rate, sale de LookAtCameraSmoothness
	float g_lookAtSmoothness = 40.0f;
	bool g_lookAtInCone = false;
	float g_lookAtTimer = 0.0f;

	// Dialogo
	constexpr float DIALOGUE_COOLDOWN = 0.75f;
	float g_dialogueCooldown = 0.0f;

	// Multiplicador para límites cuando está agachado
	// de forma vanilla ya se limita mucho la izquierda
	constexpr float SNEAK_LIMIT_MULTIPLIER = 0.5f;
	constexpr float SNEAK_EXTRA_MULTIPLIER = 0.65f;

	// Slow return
	constexpr float IDLE_SPEED_MULTIPLIER = 0.5f; // Idle: 50% más lento
	constexpr float FADE_DURATION = 2.0f;		  // 2 segundos para volver a 100%

	// smoothing
	constexpr float kMinTauSeconds = 0.02f;
	constexpr float kMaxTauSeconds = 1.2f;

	// Sandman
	std::atomic<bool> g_sandmanActive{ false };
	std::atomic<float> g_sandmanTimer{ 0.0f };
	constexpr float SANDMAN_DURATION_SECONDS = 3.5f;

	class SandmanKillSink : public RE::BSTEventSink<RE::SandmanKill::Event>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::SandmanKill::Event&,
			RE::BSTEventSource<RE::SandmanKill::Event>*) override
		{
			g_sandmanActive.store(true, std::memory_order_relaxed);
			g_sandmanTimer.store(0.0f, std::memory_order_relaxed);
			REX::LogInformation("SmoothHeadtracking: >>> SandmanKill::Event DISPARADO <<<");
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	float SmoothnessToRate(float a_smoothness0to100)
	{
		float t = std::clamp(a_smoothness0to100, 0.0f, 100.0f) / 100.0f;
		float tau = kMinTauSeconds + (kMaxTauSeconds - kMinTauSeconds) * t;
		return 1.0f / tau;
	}

	// Rutas, se cargan en este orden y cada una pisa a la anterior
	constexpr const char* kFallbackSettingsPath = "Data\\F4SE\\Plugins\\SmoothHeadtracking.ini";
	constexpr const char* kMcmDefaultsPath = "Data\\MCM\\Config\\SmoothHeadtracking\\settings.ini";
	constexpr const char* kMcmSettingsPath = "Data\\MCM\\Settings\\SmoothHeadtracking.ini";

	// el smoothness crudo (0-100), g_smoothSpeed guarda el rate ya convertido
	float g_smoothness = 15.0f;

	// si la key no esta, queda el valor que ya tenia
	float ReadFloat(CSimpleIniA& a_ini, const char* a_sec, const char* a_key, float a_cur)
	{
		std::string alt = std::string("f") + a_key;
		return static_cast<float>(a_ini.GetDoubleValue(a_sec, a_key, a_ini.GetDoubleValue(a_sec, alt.c_str(), a_cur)));
	}

	bool ReadBool(CSimpleIniA& a_ini, const char* a_sec, const char* a_key, bool a_cur)
	{
		std::string alt = std::string("b") + a_key;
		return a_ini.GetBoolValue(a_sec, a_key, a_ini.GetBoolValue(a_sec, alt.c_str(), a_cur));
	}

	int ReadInt(CSimpleIniA& a_ini, const char* a_sec, const char* a_key, int a_cur)
	{
		std::string alt = std::string("i") + a_key;
		return static_cast<int>(a_ini.GetLongValue(a_sec, a_key, a_ini.GetLongValue(a_sec, alt.c_str(), a_cur)));
	}

	// leer valore del INI
	void ApplySettingsValues(CSimpleIniA& a_ini)
	{
		const char* s = a_ini.SectionExists("Settings") ? "Settings" : "Main";
		constexpr auto r = std::memory_order_relaxed;

		g_smoothness = ReadFloat(a_ini, s, "SmoothSpeed", g_smoothness);
		g_smoothSpeed.store(SmoothnessToRate(g_smoothness), r);

		g_giveUpYawDeg.store(ReadFloat(a_ini, s, "GiveUpYawDegrees", g_giveUpYawDeg.load(r)), r);
		g_headMaxYawDeg.store(ReadFloat(a_ini, s, "HeadMaxYawDegrees", g_headMaxYawDeg.load(r)), r);
		g_spine2MaxYawDeg.store(ReadFloat(a_ini, s, "Spine2MaxYawDegrees", g_spine2MaxYawDeg.load(r)), r);
		g_maxPitchUpDeg.store(ReadFloat(a_ini, s, "MaxPitchUpDegrees", g_maxPitchUpDeg.load(r)), r);
		g_maxPitchDownDeg.store(ReadFloat(a_ini, s, "MaxPitchDownDegrees", g_maxPitchDownDeg.load(r)), r);

		g_toggleKeyVK.store(ReadInt(a_ini, s, "ToggleKeyVK", g_toggleKeyVK.load(r)), r);
		g_disableWithWeaponDrawn.store(ReadBool(a_ini, s, "DisableWithWeaponDrawn", g_disableWithWeaponDrawn.load(r)), r);
		g_idleTimeoutSeconds.store(ReadFloat(a_ini, s, "IdleTimeoutSeconds", g_idleTimeoutSeconds.load(r)), r);
		g_neckRatio.store(std::clamp(ReadFloat(a_ini, s, "NeckRatio", g_neckRatio.load(r)), 0.0f, 1.0f), r);
		g_lookAtCamera.store(ReadBool(a_ini, s, "LookAtCamera", g_lookAtCamera.load(r)), r);
		g_lookAtAngleDeg.store(std::clamp(ReadFloat(a_ini, s, "LookAtCameraAngle", g_lookAtAngleDeg.load(r)), 5.0f, 90.0f), r);
		g_lookAtDelay.store(std::max(0.0f, ReadFloat(a_ini, s, "LookAtCameraDelay", g_lookAtDelay.load(r))), r);
		g_lookAtSmoothness = ReadFloat(a_ini, s, "LookAtCameraSmoothness", g_lookAtSmoothness);
		g_lookAtSpeed.store(SmoothnessToRate(g_lookAtSmoothness), r);
	}

	bool LoadIniLayer(const char* a_path)
	{
		CSimpleIniA ini;
		ini.SetUnicode();

		if (ini.LoadFile(a_path) != SI_OK) {
			return false;
		}

		ApplySettingsValues(ini);
		return true;
	}

	// se cargan las 3 capas siempre, asi si el MCM resetea algo vuelve al default del MCM
	void LoadSettings()
	{
		const bool fallback = LoadIniLayer(kFallbackSettingsPath);
		const bool mcmDefaults = LoadIniLayer(kMcmDefaultsPath);
		const bool mcm = LoadIniLayer(kMcmSettingsPath);

		if (!fallback && !mcmDefaults && !mcm) {
			REX::LogWarning("SmoothHeadtracking: No se encontró ningún INI, usando defaults");
			return;
		}

		REX::LogInformation("SmoothHeadtracking: settings cargados (F4SE={} MCM defaults={} MCM={})", fallback, mcmDefaults, mcm);
	}

	// el MCM vive en el menu de pausa, al cerrarlo recargo
	class MenuCloseSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent& a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event.opening && a_event.menuName == RE::PauseMenu::MENU_NAME) {
				LoadSettings();
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnF4SEMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->GetType()) {
		case F4SE::MessagingInterface::MessageType::kGameDataReady:
		{
			// el UI no existe en el load del plugin, se registra aca
			static bool registered = false;
			static MenuCloseSink menuSink;
			if (!registered) {
				if (auto* ui = RE::UI::GetSingleton()) {
					registered = ui->RE::BSTEventSource<RE::MenuOpenCloseEvent>::RegisterSink(&menuSink);
				}
			}
		}
		break;
		case F4SE::MessagingInterface::MessageType::kPostLoadGame:
		case F4SE::MessagingInterface::MessageType::kNewGame:
			// por si editaron el ini a mano con el juego abierto
			LoadSettings();
			break;
		default:
			break;
		}
	}

	float smoothedYaw = 0.0f;
	float smoothedPitch = 0.0f;
	float smoothedSpine2Yaw = 0.0f;
	float smoothedNeckPitch = 0.0f;

	REL::Relocation<void (*)(PlayerCharacter*, float)> _PlayerCharacter_Update;

	// Multiplicación estándar de matrices 3x3.
	NiMatrix3 MultiplyMatrices(const NiMatrix3& a_m1, const NiMatrix3& a_m2)
	{
		NiMatrix3 result{};

		for (std::size_t i = 0; i < 3; ++i) {
			for (std::size_t j = 0; j < 3; ++j) {
				result[i][j] =
					a_m1[i][0] * a_m2[0][j] +
					a_m1[i][1] * a_m2[1][j] +
					a_m1[i][2] * a_m2[2][j];
			}
		}

		return result;
	}

	// Estructura para pasar datos de cámara calculados una sola vez
	struct CameraData
	{
		float camYaw = 0.0f;
		float camPitch = 0.0f;
		NiPoint3 camForward{ 0.0f, 0.0f, 0.0f };
	};

	// Estructura para agrupar el estado de animación y nodos cacheados
	struct AnimState
	{
		bool idleWasActive = false;
		bool wasIdle = false;
		float returnFadeTimer = 0.0f;
		NiAVObject* cachedRoot3D = nullptr;
		NiAVObject* cachedHeadNode = nullptr;
		NiAVObject* cachedSpine2Node = nullptr;
		NiAVObject* cachedNeckNode = nullptr;
		RE::TESObjectCELL* lastCell = nullptr;
	};

	// Calcula los datos de cámara una sola vez por frame
	CameraData ComputeCameraData(PlayerCamera* a_camera)
	{
		CameraData data;

		if (a_camera && a_camera->cameraRoot) {
			auto& camMat = a_camera->cameraRoot->world.rotation;
			data.camForward = NiPoint3{ camMat[1][0], camMat[1][1], camMat[1][2] };

			data.camYaw = std::atan2(-data.camForward.x, data.camForward.y);
			data.camPitch = std::asin(std::clamp(data.camForward.z, -1.0f, 1.0f));
		}

		return data;
	}

	// gunState
	// Con el arma afuera, el tracking SOLO se activa si gunState == kRelaxed
	bool IsWeaponNotRelaxed(PlayerCharacter* a_this)
	{
		bool isWeaponDrawn = (a_this->weaponState != RE::WEAPON_STATE::kSheathed);
		return isWeaponDrawn && a_this->gunState != RE::GUN_STATE::kRelaxed;
	}

	// dialogo: si cualquiera dice que si, apagado
	bool IsInDialogue(PlayerCamera* a_camera)
	{
		if (a_camera && a_camera->currentState && a_camera->currentState->id == CameraStates::kDialogue) {
			return true;
		}
		if (RE::IsPlayerInDialogue()) {
			return true;
		}
		if (auto* mtm = RE::MenuTopicManager::GetSingleton(); mtm && (mtm->menuOpen || mtm->speaker)) {
			return true;
		}
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen<RE::DialogueMenu>().value_or(false)) {
			return true;
		}
		return false;
	}

	// Detección de idle
	bool IsPlayerIdle(PlayerCharacter* a_this, const CameraData& a_camData, float a_delta)
	{
		float idleTimeout = g_idleTimeoutSeconds.load(std::memory_order_relaxed);

		if (idleTimeout <= 0.0f) {
			g_idleTimer = 0.0f;
			return false;
		}

		// Detectar movimiento de posición
		NiPoint3 currentPos = a_this->GetPosition();

		if (g_lastPlayerPosition == NiPoint3{ 0.0f, 0.0f, 0.0f }) {
			g_lastPlayerPosition = currentPos;
			return false;
		}

		float distance = (currentPos - g_lastPlayerPosition).Length();
		g_lastPlayerPosition = currentPos;

		// Detectar movimiento de cámara
		float yawDelta = std::abs(a_camData.camYaw - g_lastCamYaw);
		float pitchDelta = std::abs(a_camData.camPitch - g_lastCamPitch);

		g_lastCamYaw = a_camData.camYaw;
		g_lastCamPitch = a_camData.camPitch;

		bool camMoved = (yawDelta > IDLE_CAM_EPSILON || pitchDelta > IDLE_CAM_EPSILON);

		bool hasActivity = (distance > IDLE_POSITION_EPSILON) || camMoved;

		// las teclas solo si no se movio nada
		if (!hasActivity) {
			for (int key : std::initializer_list<int>{ 'W', 'A', 'S', 'D', VK_SHIFT, VK_CONTROL, VK_SPACE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT }) {
				if (GetAsyncKeyState(key) & 0x8000) {
					hasActivity = true;
					break;
				}
			}
		}

		if (hasActivity) {
			g_idleTimer = 0.0f;
			return false;
		}

		g_idleTimer += a_delta;

		return g_idleTimer >= idleTimeout;
	}

	// Yaw de la camara relativo a la orientacion real del cuerpo, normalizado a [-PI, PI].
	// el yaw del cuerpo se lee del esqueleto (root3D->world.rotation), NO de
	// a_this->data.angle.z, ese angulo tendio a desincronizar en demasiados edge-case.
	float ComputeRelativeYaw(NiAVObject* a_root3D, float a_camYaw)
	{
		NiPoint3 bodyForward{ a_root3D->world.rotation[1][0], a_root3D->world.rotation[1][1], a_root3D->world.rotation[1][2] };
		float playerYaw = std::atan2(-bodyForward.x, bodyForward.y);

		float relativeYaw = a_camYaw - playerYaw;

		while (relativeYaw > PI) {
			relativeYaw -= 2.0f * PI;
		}
		while (relativeYaw < -PI) {
			relativeYaw += 2.0f * PI;
		}

		return relativeYaw;
	}

	// condiciones en los que el tracking de cabeza y spine2 (Futuramente Neck) debe
	// quedar completamente apagado.
	bool IsTrackingDisabled(PlayerCharacter* a_this, PlayerCamera* a_camera, const CameraData& a_camData, float a_relativeYaw, float a_delta, bool& a_isIdle, bool& a_isInGiveUp)
	{
		bool isBadCameraState = a_camera && a_camera->currentState && (a_camera->currentState->id == CameraStates::kDialogue || a_camera->currentState->id == CameraStates::kVATS || a_camera->currentState->id == CameraStates::kFurniture || a_camera->currentState->id == CameraStates::kBleedout || a_camera->currentState->id == CameraStates::kFirstPerson || a_camera->currentState->id == CameraStates::kIronSights);

		bool isInFurniture = a_this->currentProcess && a_this->currentProcess->GetOccupiedFurniture().get() != nullptr;

		bool isWeaponDrawnDisabled = g_disableWithWeaponDrawn.load(std::memory_order_relaxed) ? (a_this->weaponState != RE::WEAPON_STATE::kSheathed) : IsWeaponNotRelaxed(a_this);

		bool isSandmanActive = g_sandmanActive.load(std::memory_order_relaxed);

		// Detectar si estamos en Give Up para no aplicar idle
		float giveUpYaw = g_giveUpYawDeg.load(std::memory_order_relaxed) * (PI / 180.0f);
		a_isInGiveUp = std::abs(a_relativeYaw) > giveUpYaw;

		// Idle detection: solo si NO estamos en Give Up
		a_isIdle = false;
		if (!a_isInGiveUp) {
			a_isIdle = IsPlayerIdle(a_this, a_camData, a_delta);
		}

		return a_this->IsDead(false) || isBadCameraState || isInFurniture || isWeaponDrawnDisabled || RE::IsPlayerInDialogue() || a_isIdle || isSandmanActive;
	}

	// Reparte el yaw relativo entre cabeza y spine2 (cabeza satura primero, el
	// remanente hasta spine2MaxYaw se lo lleva spine2), y aplica el hasGivenUp /
	// trackingGate: mas alla de giveUpYawDeg el tracking se "suelta" con histeresis,
	// y trackingGate suaviza esa transicion en vez de cortar el target de golpe.
	// El hecho del GiveUp es porque obviamente en tercera persona quiero
	// ver la carita de mi PJ.
	void SplitYaw(float a_yaw, float& a_head, float& a_spine2)
	{
		float headMaxYaw = g_headMaxYawDeg.load(std::memory_order_relaxed) * (PI / 180.0f);
		float spine2MaxYaw = g_spine2MaxYawDeg.load(std::memory_order_relaxed) * (PI / 180.0f);

		float sign = a_yaw < 0.0f ? -1.0f : 1.0f;
		float remaining = std::abs(a_yaw);

		float useHead = std::min(remaining, headMaxYaw);
		remaining -= useHead;

		float useSpine2 = std::min(remaining, spine2MaxYaw);

		a_head = sign * useHead;
		a_spine2 = sign * useSpine2;
	}

	void ComputeYawTargets(float a_relativeYaw, float a_delta, bool a_forceGiveUp, float& a_targetHeadYaw, float& a_targetSpine2Yaw, float& a_trackingGate, bool& a_hasGivenUp)
	{
		float smoothSpeed = g_smoothSpeed.load(std::memory_order_relaxed);
		float giveUpYawDeg = g_giveUpYawDeg.load(std::memory_order_relaxed);
		constexpr float HYSTERESIS_DEG = 15.0f;

		float giveUpYaw = giveUpYawDeg * (PI / 180.0f);
		float reengageYaw = (giveUpYawDeg - HYSTERESIS_DEG) * (PI / 180.0f);

		static bool hasGivenUp = false;
		static float trackingGate = 1.0f;

		float absYaw = std::abs(a_relativeYaw);

		if (!hasGivenUp && absYaw > giveUpYaw) {
			hasGivenUp = true;
		}
		else if (hasGivenUp && absYaw < reengageYaw) {
			hasGivenUp = false;
		}

		// a_forceGiveUp: cuando mira a la camara, la mira se suelta si o si
		float gateTarget = (hasGivenUp || a_forceGiveUp) ? 0.0f : 1.0f;
		float gateT = 1.0f - std::exp(-smoothSpeed * a_delta);
		trackingGate += (gateTarget - trackingGate) * gateT;

		float head = 0.0f;
		float spine2 = 0.0f;
		SplitYaw(a_relativeYaw, head, spine2);

		a_targetHeadYaw = head * trackingGate;
		a_targetSpine2Yaw = spine2 * trackingGate;
		a_trackingGate = trackingGate;
		a_hasGivenUp = hasGivenUp;
	}

	// hacia donde esta la camara visto desde la cabeza
	void ComputeLookAtCameraTargets(NiAVObject* a_root3D, NiAVObject* a_headNode, PlayerCamera* a_camera, float& a_relYaw, float& a_pitch)
	{
		NiPoint3 toCam = a_camera->cameraRoot->world.translation - a_headNode->world.translation;
		float horiz = std::sqrt(toCam.x * toCam.x + toCam.y * toCam.y);

		if (horiz < 1.0f) {
			a_relYaw = 0.0f;
			a_pitch = 0.0f;
			return;
		}

		float camDirYaw = std::atan2(-toCam.x, toCam.y);

		NiPoint3 bodyForward{ a_root3D->world.rotation[1][0], a_root3D->world.rotation[1][1], a_root3D->world.rotation[1][2] };
		float playerYaw = std::atan2(-bodyForward.x, bodyForward.y);

		float rel = camDirYaw - playerYaw;
		while (rel > PI) {
			rel -= 2.0f * PI;
		}
		while (rel < -PI) {
			rel += 2.0f * PI;
		}

		a_relYaw = rel;
		a_pitch = std::atan2(toCam.z, horiz);
	}

	void PlayerCharacter_Update_Hook(PlayerCharacter* a_this, float a_delta)
	{
		if (!a_this) {
			_PlayerCharacter_Update(a_this, a_delta);
			return;
		}

		// Validación de a_delta
		if (a_delta <= 0.0f || a_delta > 1.0f) {
			a_delta = 0.016f; // ~60 FPS
		}

		if (g_sandmanActive.load(std::memory_order_relaxed)) {
			float t = g_sandmanTimer.load(std::memory_order_relaxed) + a_delta;
			if (t >= SANDMAN_DURATION_SECONDS) {
				g_sandmanActive.store(false, std::memory_order_relaxed);
				g_sandmanTimer.store(0.0f, std::memory_order_relaxed);
				REX::LogInformation("SmoothHeadtracking: <<< Sandman terminado, tracking reanudado >>>");
			}
			else {
				g_sandmanTimer.store(t, std::memory_order_relaxed);
			}
		}

		_PlayerCharacter_Update(a_this, a_delta);

		int toggleKeyVK = g_toggleKeyVK.load(std::memory_order_relaxed);
		if (toggleKeyVK != 0) {
			static bool wasKeyDown = false;
			bool isKeyDown = (GetAsyncKeyState(toggleKeyVK) & 0x8000) != 0;
			if (isKeyDown && !wasKeyDown) {
				bool newState = !g_trackingEnabled.load(std::memory_order_relaxed);
				g_trackingEnabled.store(newState, std::memory_order_relaxed);
			}
			wasKeyDown = isKeyDown;
		}

		auto camera = RE::PlayerCamera::GetSingleton();

		if (!camera || !camera->cameraRoot) {
			return;
		}

		auto root3D = a_this->Get3D();
		if (!root3D) {
			return;
		}

		// Estado de animación y nodos cacheados
		static AnimState animState;

		// Detectar cambio de cell
		auto* currentCell = a_this->GetParentCell();
		if (currentCell != animState.lastCell) {
			// Resetear valores suavizados
			smoothedYaw = 0.0f;
			smoothedPitch = 0.0f;
			smoothedSpine2Yaw = 0.0f;
			smoothedNeckPitch = 0.0f;
			g_lookAtCamWeight = 0.0f;
			g_lookAtInCone = false;
			g_lookAtTimer = 0.0f;

			// Resetear estado de idle
			g_idleTimer = 0.0f;
			animState.returnFadeTimer = 0.0f;

			// Invalidar cache de nodos
			animState.cachedRoot3D = nullptr;
			animState.cachedHeadNode = nullptr;
			animState.cachedSpine2Node = nullptr;
			animState.cachedNeckNode = nullptr;

			animState.lastCell = currentCell;
		}

		// Invalidar cache si el 3D cambió (equipo, etc.)
		if (root3D != animState.cachedRoot3D) {
			animState.cachedRoot3D = root3D;
			animState.cachedHeadNode = nullptr;
			animState.cachedSpine2Node = nullptr;
			animState.cachedNeckNode = nullptr;
		}

		// Obtener nodos del cache o buscarlos
		auto headNode = animState.cachedHeadNode;
		if (!headNode) {
			headNode = root3D->GetObjectByName("NPC Head [Head]");
			if (!headNode) {
				headNode = root3D->GetObjectByName("Head");
			}
			animState.cachedHeadNode = headNode;
		}

		auto spine2Node = animState.cachedSpine2Node;
		if (!spine2Node) {
			spine2Node = root3D->GetObjectByName("SPINE2");
			animState.cachedSpine2Node = spine2Node;
		}

		auto neckNode = animState.cachedNeckNode;
		if (!neckNode) {
			neckNode = root3D->GetObjectByName("Neck");
			if (!neckNode) {
				neckNode = root3D->GetObjectByName("NPC Neck [Neck]");
			}
			animState.cachedNeckNode = neckNode;
		}

		if (!headNode || !headNode->parent) {
			return;
		}

		// Calcular datos de cámara una sola vez por frame
		CameraData camData = ComputeCameraData(camera);
		float relativeYaw = ComputeRelativeYaw(root3D, camData.camYaw);

		// dialogo: corte duro, no toco nada
		if (IsInDialogue(camera)) {
			g_dialogueCooldown = DIALOGUE_COOLDOWN;
		}
		else if (g_dialogueCooldown > 0.0f) {
			g_dialogueCooldown -= a_delta;
		}

		if (g_dialogueCooldown > 0.0f) {
			smoothedYaw = 0.0f;
			smoothedPitch = 0.0f;
			smoothedSpine2Yaw = 0.0f;
			smoothedNeckPitch = 0.0f;
			g_lookAtCamWeight = 0.0f;
			g_lookAtInCone = false;
			g_lookAtTimer = 0.0f;
			return;
		}

		bool isIdle = false;
		bool isInGiveUp = false;

		// Evaluar IsTrackingDisabled SIEMPRE, sin corto-circuito
		bool trackingDisabledByOtherReasons = IsTrackingDisabled(a_this, camera, camData, relativeYaw, a_delta, isIdle, isInGiveUp);
		bool isDisabled = !g_trackingEnabled.load(std::memory_order_relaxed) || trackingDisabledByOtherReasons;

		float targetHeadYaw = 0.0f;
		float targetSpine2Yaw = 0.0f;
		float clampedPitch = 0.0f;
		float targetNeckPitch = 0.0f;

		float smoothSpeed = g_smoothSpeed.load(std::memory_order_relaxed);
		float neckRatio = g_neckRatio.load(std::memory_order_relaxed);

		// safe
		if (!neckNode) {
			neckRatio = 0.0f;
		}

		if (!isDisabled) {
			float maxPitchUp = g_maxPitchUpDeg.load(std::memory_order_relaxed) * (PI / 180.0f);
			float maxPitchDown = g_maxPitchDownDeg.load(std::memory_order_relaxed) * (PI / 180.0f);
			float headMaxYaw = g_headMaxYawDeg.load(std::memory_order_relaxed) * (PI / 180.0f);
			float spine2MaxYaw = g_spine2MaxYawDeg.load(std::memory_order_relaxed) * (PI / 180.0f);

			constexpr auto r = std::memory_order_relaxed;

			// Look at camera
			// hacia donde ESTA la camara (su posicion), no hacia donde mira
			float camRelYaw = 0.0f, camPitch = 0.0f;
			ComputeLookAtCameraTargets(root3D, headNode, camera, camRelYaw, camPitch);

			// cono de enfrente, con histeresis pa que no titile en el borde
			const float coneEnter = g_lookAtAngleDeg.load(r) * (PI / 180.0f);
			const float coneExit = coneEnter + 10.0f * (PI / 180.0f);
			const float absCam = std::abs(camRelYaw);

			if (!g_lookAtCamera.load(r)) {
				g_lookAtInCone = false;
			}
			else if (!g_lookAtInCone && absCam < coneEnter) {
				g_lookAtInCone = true;
			}
			else if (g_lookAtInCone && absCam > coneExit) {
				g_lookAtInCone = false;
			}

			// mientras te mira (o va saliendo) la mira queda suelta,
			// asi siempre pasa por el centro y no cruza de un lado al otro
			const bool forceGiveUp = g_lookAtInCone || g_lookAtCamWeight > 0.05f;

			float trackingGate = 1.0f;
			bool hasGivenUp = false;
			ComputeYawTargets(relativeYaw, a_delta, forceGiveUp, targetHeadYaw, targetSpine2Yaw, trackingGate, hasGivenUp);

			float rawPitch = std::clamp(camData.camPitch, -maxPitchDown, maxPitchUp);
			clampedPitch = rawPitch * trackingGate;

			// primero vuelve al centro, espera un ratito y recien ahi te mira
			bool wantLook = false;
			if (g_lookAtInCone && trackingGate < 0.1f) {
				g_lookAtTimer += a_delta;
				wantLook = g_lookAtTimer >= g_lookAtDelay.load(r);
			}
			else {
				g_lookAtTimer = 0.0f;
			}

			const float lookT = 1.0f - std::exp(-g_lookAtSpeed.load(r) * a_delta);
			g_lookAtCamWeight += ((wantLook ? 1.0f : 0.0f) - g_lookAtCamWeight) * lookT;

			if (g_lookAtCamWeight > 0.001f) {
				float camHead = 0.0f, camSpine2 = 0.0f;
				SplitYaw(camRelYaw, camHead, camSpine2);
				camPitch = std::clamp(camPitch, -maxPitchDown, maxPitchUp);

				const float w = g_lookAtCamWeight;
				targetHeadYaw = targetHeadYaw * (1.0f - w) + camHead * w;
				targetSpine2Yaw = targetSpine2Yaw * (1.0f - w) + camSpine2 * w;
				clampedPitch = clampedPitch * (1.0f - w) + camPitch * w;
			}

			// límites proporcionales cuando está agachado
			if (a_this->IsSneaking()) {
				// Multiplicador base: 50%
				float multiplier = SNEAK_LIMIT_MULTIPLIER;

				// Si está mirando hacia un lado extremo, permitir un poco más
				// (compensa la animación de agachado que ya gira el cuerpo)
				float absRelYaw = std::abs(relativeYaw);
				if (absRelYaw > 0.785f) {
					multiplier = SNEAK_EXTRA_MULTIPLIER;
				}

				float sneakPitchUp = maxPitchUp * multiplier;
				float sneakPitchDown = maxPitchDown * multiplier;
				float sneakHeadYaw = headMaxYaw * multiplier;
				float sneakSpine2Yaw = spine2MaxYaw * multiplier;

				// Limitar pitch
				clampedPitch = std::clamp(clampedPitch, -sneakPitchDown, sneakPitchUp);

				// Limitar yaw de Head
				targetHeadYaw = std::clamp(targetHeadYaw, -sneakHeadYaw, sneakHeadYaw);

				// Limitar yaw de Spine2
				targetSpine2Yaw = std::clamp(targetSpine2Yaw, -sneakSpine2Yaw, sneakSpine2Yaw);
			}

			// Split para Neck: SOLO PITCH se reparte
			float neckPitchTotal = clampedPitch * neckRatio;
			targetNeckPitch = neckPitchTotal;

			clampedPitch = clampedPitch * (1.0f - neckRatio);
		}

		// Si idle está activo, guardamos el estado
		if (isIdle) {
			animState.idleWasActive = true;
		}

		// Si estamos en GiveUp y el idle estaba activo, mantener idle activo
		if (isInGiveUp && animState.idleWasActive) {
			isIdle = true;
		}

		// Si salimos de GiveUp y el idle ya no está activo, resetear
		if (!isInGiveUp && !isIdle) {
			animState.idleWasActive = false;
		}

		// slow return con fade gradual
		// Detectar si el tracking se acaba de reactivar
		bool justReactivated = (animState.wasIdle && !isIdle);
		animState.wasIdle = isIdle;

		float currentSpeedMultiplier = 1.0f;

		if (isIdle) {
			animState.returnFadeTimer = 0.0f;
			currentSpeedMultiplier = IDLE_SPEED_MULTIPLIER;
		}
		else if (justReactivated) {
			animState.returnFadeTimer = 0.0f;
			currentSpeedMultiplier = IDLE_SPEED_MULTIPLIER;
		}
		else if (!isIdle && animState.returnFadeTimer < FADE_DURATION) {
			animState.returnFadeTimer += a_delta;
			float progress = animState.returnFadeTimer / FADE_DURATION;
			progress = std::min(progress, 1.0f);

			float fadeMultiplier = IDLE_SPEED_MULTIPLIER + (1.0f - IDLE_SPEED_MULTIPLIER) * progress;
			currentSpeedMultiplier = fadeMultiplier;
		}
		else {
			// Normal
			animState.returnFadeTimer = FADE_DURATION;
			currentSpeedMultiplier = 1.0f;
		}

		// Aplicar interpolación
		float effectiveSpeed = smoothSpeed * currentSpeedMultiplier;
		float t = 1.0f - std::exp(-effectiveSpeed * a_delta);
		float tNormal = 1.0f - std::exp(-smoothSpeed * a_delta);

		// Interpola siempre hacia los targets (incluso con tracking apagado, para
		// que la cabeza vuelva a la pose neutral de forma suave y no de un corte).
		if (isIdle) {
			smoothedYaw += (0.0f - smoothedYaw) * t;
			smoothedPitch += (0.0f - smoothedPitch) * t;
			smoothedSpine2Yaw += (0.0f - smoothedSpine2Yaw) * t;
			smoothedNeckPitch += (0.0f - smoothedNeckPitch) * t;
		}
		else {
			smoothedYaw += (targetHeadYaw - smoothedYaw) * tNormal;
			smoothedPitch += (clampedPitch - smoothedPitch) * tNormal;
			smoothedSpine2Yaw += (targetSpine2Yaw - smoothedSpine2Yaw) * tNormal;
			smoothedNeckPitch += (targetNeckPitch - smoothedNeckPitch) * tNormal;
		}

		// Si las rotaciones ya están prácticamente en cero y el tracking está deshabilitado,
		// reseteamos totalmente los valores y no sobreescribimos la matriz.
		// en FNV no pasa esto jeje.
		constexpr float EPSILON = 0.001f;
		if (isDisabled &&
			std::abs(smoothedYaw) < EPSILON &&
			std::abs(smoothedPitch) < EPSILON &&
			std::abs(smoothedSpine2Yaw) < EPSILON &&
			std::abs(smoothedNeckPitch) < EPSILON) {
			smoothedYaw = 0.0f;
			smoothedPitch = 0.0f;
			smoothedSpine2Yaw = 0.0f;
			smoothedNeckPitch = 0.0f;
			return;
		}

		// el nodo mas alto que toque, desde ahi un solo update
		NiAVObject* topNode = nullptr;

		// Modificar SPINE2 solo si hay un desfase significativo.
		if (spine2Node && std::abs(smoothedSpine2Yaw) >= EPSILON) {
			NiMatrix3 baseSpine2Rot = spine2Node->local.rotation;

			// esta wea fue a puro prueba y error.
			NiMatrix3 rotSpine2;
			rotSpine2.FromEulerAnglesXYZ(-smoothedSpine2Yaw, 0.0f, 0.0f);

			spine2Node->local.rotation = MultiplyMatrices(rotSpine2, baseSpine2Rot);
			topNode = spine2Node;
		}

		// Modificar NECK solo si existe y hay un desfase significativo en pitch
		if (neckNode && std::abs(smoothedNeckPitch) >= EPSILON) {
			NiMatrix3 baseNeckRot = neckNode->local.rotation;

			NiMatrix3 rotNeckPitch;
			rotNeckPitch.FromEulerAnglesXYZ(0.0f, 0.0f, smoothedNeckPitch);

			neckNode->local.rotation = MultiplyMatrices(rotNeckPitch, baseNeckRot);
			if (!topNode) {
				topNode = neckNode;
			}
		}

		// Modificar HEAD solo si hay un desfase significativo.
		if (std::abs(smoothedYaw) >= EPSILON || std::abs(smoothedPitch) >= EPSILON) {
			NiMatrix3 baseAnimLocalRot = headNode->local.rotation;

			// esto tambien fue a puro prueba y error.
			NiMatrix3 rotYaw;
			rotYaw.FromEulerAnglesXYZ(-smoothedYaw, 0.0f, 0.0f);

			NiMatrix3 rotPitch;
			rotPitch.FromEulerAnglesXYZ(0.0f, 0.0f, smoothedPitch);

			NiMatrix3 rotCombined = MultiplyMatrices(rotPitch, rotYaw);

			headNode->local.rotation = MultiplyMatrices(rotCombined, baseAnimLocalRot);
			if (!topNode) {
				topNode = headNode;
			}
		}

		if (topNode) {
			NiUpdateData updateData{};
			topNode->UpdateTransforms(updateData);
		}
	}

	// Funcion nativa expuesta a Papyrus para el hotkey del MCM.
	void TogglePapyrus(std::monostate)
	{
		bool newState = !g_trackingEnabled.load(std::memory_order_relaxed);
		g_trackingEnabled.store(newState, std::memory_order_relaxed);
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->BindNativeFunction("SmoothHT_MCM", "Toggle", TogglePapyrus, false);
		return true;
	}

	void InstallHooks()
	{
		REL::HookVft hook{
			VTABLE::PlayerCharacter[0],
			0xCF,
			PlayerCharacter_Update_Hook
		};

		_PlayerCharacter_Update =
			reinterpret_cast<std::uintptr_t>(hook.get());

		hook.Init();
		hook.Enable();
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	LoadSettings();

	InstallHooks();
	F4SE::GetPapyrusInterface()->Register(RegisterPapyrusFunctions);
	F4SE::GetMessagingInterface()->RegisterListener(OnF4SEMessage);

	// --- sandman Perk ---
	static SandmanKillSink sandmanSink;
	if (auto* source = RE::SandmanKill::Event::GetEventSource()) {
		source->RegisterSink(&sandmanSink);
		REX::LogInformation("SmoothHeadtracking: SandmanKillSink registrado OK.");
	}
	else {
		REX::LogWarning("SmoothHeadtracking: GetEventSource() devolvio nullptr.");
	}

	return true;
}