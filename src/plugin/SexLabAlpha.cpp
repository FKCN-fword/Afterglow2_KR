#include "SexLabAlpha.h"

#include "Settings.h"

namespace AG::SexLabAlpha
{
	namespace
	{
		constexpr auto kSetting = "fCumAlpha";

		class OnValue : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			explicit OnValue(std::function<void(float)> a_fn) :
				_fn(std::move(a_fn)) {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				if (a_result.IsFloat()) {
					_fn(a_result.GetFloat());
				}
			}
			void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

		private:
			std::function<void(float)> _fn;
		};

		void SetValue(float a_value)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
			auto*                                                     args = RE::MakeFunctionArguments(RE::BSFixedString(kSetting), static_cast<float>(a_value));
			if (vm && vm->DispatchStaticCall("sslSystemConfig", "SetSettingFlt", args, none)) {
				logger::info("SexLab P+ {} set to {:.2f}", kSetting, a_value);
			}
		}
	}

	void Sync()
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb(new OnValue([](float a_current) {
			auto& s = Settings::Get();
			if (s.hideSexLabOverlays) {
				if (a_current > 0.001f) {
					s.sexLabAlphaBefore = a_current;
					s.Save();
					SetValue(0.0f);
				}
			} else if (s.sexLabAlphaBefore >= 0.0f) {
				const float restore = s.sexLabAlphaBefore;
				s.sexLabAlphaBefore = -1.0f;
				s.Save();
				SetValue(restore);
			}
		}));
		auto* args = RE::MakeFunctionArguments(RE::BSFixedString(kSetting));
		if (!vm->DispatchStaticCall("sslSystemConfig", "GetSettingFlt", args, cb)) {
			logger::info("SexLab P+ not found (sslSystemConfig.GetSettingFlt), its overlays are left alone");
		}
	}
}
