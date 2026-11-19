#include "Menu.h"

#include "Body.h"
#include "Manager.h"
#include "Settings.h"
#include "SexLabAlpha.h"
#include "Skee.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace AG::Menu
{
	namespace
	{
		// English / Korean UI text, switched by bKorean in Afterglow.ini (or the checkbox at the top of the page)
		const char* T(const char* a_en, const char* a_ko)
		{
			return Settings::Get().korean ? a_ko : a_en;
		}

		void Help(const char* a_text)
		{
			if (a_text && ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", a_text);
			}
		}

		void Toggle(const char* a_label, bool& a_value, const char* a_help = nullptr)
		{
			if (ImGuiMCP::Checkbox(a_label, std::addressof(a_value))) {
				Settings::Get().Save();
			}
			Help(a_help);
		}

		RE::Actor* Target(bool a_selected)
		{
			if (a_selected) {
				if (auto ref = RE::Console::GetSelectedRef(); ref) {
					return ref->As<RE::Actor>();
				}
				return nullptr;
			}
			return RE::PlayerCharacter::GetSingleton();
		}

		void __stdcall Settings_()
		{
			auto& s = Settings::Get();
			if (Manager::Blocked()) {
				ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", Manager::BlockReason().data());
			}
			if (!Skee::Ready()) {
				ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", Skee::Status().data());
			}

			Toggle("Korean UI / 한국어", s.korean, "Off = English menu text (use this if the Korean text shows as ? or boxes).");

			ImGuiMCP::SeparatorText(T("Look", "외형"));
			if (ImGuiMCP::SliderFloat(T("Opacity", "불투명도"), std::addressof(s.opacity), 0.1f, 1.0f, "%.2f")) {
				s.Save();
			}
			Help(T("Opacity of new cum layers.", "새로 생기는 레이어의 불투명도입니다."));
			if (ImGuiMCP::SliderFloat(T("Wet shine: tightness", "젖은 광택: 집중도"), std::addressof(s.glossiness), 0.0f, 300.0f, "%.0f")) {
				s.Save();
			}
			Help(T("Specular power of new layers. Higher = smaller, sharper highlight. 0 = same as the skin.", "새 레이어의 스페큘러 파워입니다. 높을수록 하이라이트가 작고 날카로워집니다. 0 = 피부와 동일."));
			if (ImGuiMCP::SliderFloat(T("Wet shine: strength", "젖은 광택: 강도"), std::addressof(s.specular), 0.0f, 10.0f, "%.1f")) {
				s.Save();
			}
			Help(T("Specular strength of new layers. 0 = same as the skin.", "새 레이어의 스페큘러 강도입니다. 0 = 피부와 동일."));
			if (ImGuiMCP::SliderFloat(T("Dries after (game hours)", "마르는 시간 (게임 시간)"), std::addressof(s.dryHours), 0.0f, 168.0f, "%.0f")) {
				s.Save();
			}
			Help(T("0 = stays until washed or cleared by SexLab.", "0 = 씻기거나 SexLab이 지울 때까지 유지됩니다."));
			if (ImGuiMCP::SliderFloat(T("Fade-out (seconds)", "페이드아웃 (초)"), std::addressof(s.fadeSeconds), 0.0f, 5.0f, "%.1f")) {
				s.Save();
			}

			ImGuiMCP::SeparatorText(T("Layers per zone (0 = off)", "부위별 레이어 수 (0 = 끔)"));
			const std::array<const char*, kZoneCount> kLabels{ T("Vaginal", "질"), T("Anal", "항문"), T("Breast", "가슴"), T("Face", "얼굴"), T("Hands", "손"), T("Feet", "발"), T("Legs (footjob)", "다리 (풋잡)") };
			for (std::size_t z = 0; z < kZoneCount; ++z) {
				ImGuiMCP::SetNextItemWidth(200.0f);
				if (ImGuiMCP::SliderInt(kLabels[z], std::addressof(s.maxLayers[z]), 0, 12)) {
					s.Save();
				}
			}

			ImGuiMCP::SeparatorText(T("Who and when", "대상과 조건"));
			Toggle(T("Male targets", "남성 대상"), s.maleTargets, T("Men can receive cum layers too (by default only women, futa and female creatures).", "남성도 레이어를 받을 수 있습니다. (기본값: 여성, 퓨타, 여성 크리처만)"));
			Toggle(T("Follow SexLab cleanup", "SexLab 정리 따라가기"), s.followSexLabCleanup, T("Off (default): every zone dries on Afterglow's own timer above. On: when SexLab P+ removes its own cum (swimming, its fCumTimer), Afterglow also removes that zone - SexLab only knows vaginal, anal and oral, so breast, hands and feet still use Afterglow's timer.", "끔(기본값): 모든 부위가 위의 Afterglow 자체 타이머로 마릅니다. 켬: SexLab P+가 자신의 정액을 지울 때(수영, fCumTimer) Afterglow도 해당 부위를 지웁니다. SexLab은 질, 항문, 구강만 알기 때문에 가슴, 손, 발은 계속 Afterglow의 타이머를 따릅니다."));
			Toggle(T("Wash with Bathing in Skyrim / Dirt and Blood", "Bathing in Skyrim / Dirt and Blood로 씻기"), s.washOnBathing);
			bool hide = s.hideSexLabOverlays;
			if (ImGuiMCP::Checkbox(T("Hide SexLab P+'s own cum overlays", "SexLab P+의 기본 정액 오버레이 숨기기"), std::addressof(hide))) {
				s.hideSexLabOverlays = hide;
				s.Save();
				SKSE::GetTaskInterface()->AddTask([]() { SexLabAlpha::Sync(); });
			}
			Help(T("Sets SexLab P+'s fCumAlpha to 0, so only Afterglow draws. SexLab's spells and cum counts keep working. Turning it off restores your old value.", "SexLab P+의 fCumAlpha를 0으로 만들어 Afterglow만 그리게 합니다. SexLab의 주문과 카운트는 계속 동작합니다. 끄면 이전 값으로 복원됩니다."));
			const char* kFallback[] = { T("Vaginal", "질"), T("Anal", "항문"), T("Oral", "구강") };
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::Combo(T("Untagged scene fallback", "태그 없는 씬 기본 부위"), std::addressof(s.fallbackZone), kFallback, 3)) {
				s.Save();
			}
			Help(T("A scene without cum tags where SexLab could not tell the type either (mostly creatures).", "정액 태그가 없고 SexLab도 종류를 판단하지 못한 씬(주로 크리처)에 적용됩니다."));
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::SliderInt(T("First overlay slot", "첫 오버레이 슬롯"), std::addressof(s.startSlot), 0, 40)) {
				s.Save();
			}
			Help(T("Afterglow only uses free slots from here on and never paints over tattoos or other mods' overlays.", "Afterglow는 이 번호부터 빈 슬롯만 사용하며, 문신이나 다른 모드의 오버레이 위에는 칠하지 않습니다."));
			Toggle(T("Debug log", "디버그 로그"), s.debugLog, T("Every layer in Afterglow.log.", "모든 레이어를 Afterglow.log에 기록합니다."));
			spdlog::default_logger()->set_level(s.debugLog ? spdlog::level::debug : spdlog::level::info);
		}

		void TestRow(const char* a_id, const char* a_shown, bool a_selected)
		{
			ImGuiMCP::PushID(a_id);
			ImGuiMCP::TextDisabled("%s", a_shown);
			struct B
			{
				const char* label;
				ZoneSet     zones;
			};
			const std::array<B, 7> kButtons{ { { T("Vaginal", "질"), Bit(Zone::Vaginal) }, { T("Anal", "항문"), Bit(Zone::Anal) }, { T("Oral", "구강"), Bit(Zone::Face) | Bit(Zone::Breast) },
				{ T("Breast", "가슴"), Bit(Zone::Breast) }, { T("Hands", "손"), Bit(Zone::Hands) }, { T("Feet", "발"), Bit(Zone::Feet) | Bit(Zone::Legs) }, { T("All", "전체"), ~ZoneSet{ 0 } } } };
			for (const auto& b : kButtons) {
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(b.label)) {
					Manager::Test(Target(a_selected), b.zones);
				}
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(T("Clear", "지우기"))) {
				Manager::ClearAll(Target(a_selected));
			}
			ImGuiMCP::PopID();
		}

		void __stdcall Status()
		{
			ImGuiMCP::SeparatorText(T("Status", "상태"));
			ImGuiMCP::TextWrapped("%s", Skee::Status().data());
			ImGuiMCP::TextWrapped(T("Actors with cum: %zu", "레이어가 있는 대상: %zu"), Manager::TrackedActors());
			ImGuiMCP::TextWrapped("%s", Manager::Describe(Target(false)).c_str());
			if (auto* sel = Target(true); sel && sel != Target(false)) {
				ImGuiMCP::TextWrapped("%s", Manager::Describe(sel).c_str());
			}
			ImGuiMCP::SeparatorText(T("Test", "테스트"));
			TestRow("Player", T("Player", "플레이어"), false);
			TestRow("Console selection", T("Console selection", "콘솔 선택 대상"), true);
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSE Menu Framework not installed: no settings page (Afterglow.ini still works)");
			return;
		}
		SKSEMenuFramework::SetSection("Afterglow");
		SKSEMenuFramework::AddSectionItem(T("Settings", "설정"), Settings_);
		SKSEMenuFramework::AddSectionItem(T("Status and test", "상태 및 테스트"), Status);
	}
}
