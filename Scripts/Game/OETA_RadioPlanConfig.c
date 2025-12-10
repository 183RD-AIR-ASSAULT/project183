// -----------------------------------------------------------------------------
// OETA_RadioPlanConfig.c
// Config containers for channel plans (.conf root).
//
// Each OETA_RadioPlanConfig:
//
//   Name               = plan name (e.g. "ABLE")
//   BaseFrequenciesKHz = "68500 69500" (space-separated; optional)
//   Slots[]            = per-channel freq + label, from the configured start
//                        channel in OETA_RadioAutoTuneComponent.
//
// Multiple configs with the same Name are merged at runtime; their base freq
// lists are aggregated so many group freqs can trigger the same plan.
// -----------------------------------------------------------------------------

[BaseContainerProps()]
class OETA_RadioPlanSlotConfig
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox,
		desc: "Slot frequency in kHz (0 = unchanged)")]
	int FrequencyKHz;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox,
		desc: "Channel text/name (optional)")]
	string Text;
}

[BaseContainerProps()]
class OETA_RadioPlanConfig
{
	// Space-separated list of base group freqs (kHz) which trigger this plan.
	// Empty string = no auto-match; plan can still be used via override.
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox,
		desc: "Base group freqs (kHz) for this plan (space-separated, e.g. '68500 69500')")]
	string BaseFrequenciesKHz;

	// Name that ties all parts of a plan together.
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox,
		desc: "Plan name (plans with the same name are merged at runtime)")]
	string Name;

	// Only one config for a given Name needs to provide Slots. Others can leave empty.
	[Attribute("", uiwidget: UIWidgets.Object,
		desc: "Slots for channels starting at the plan start index")]
	ref array<ref OETA_RadioPlanSlotConfig> Slots;
}

[BaseContainerProps(configRoot: true)]
class OETA_RadioPlanConfigRoot
{
	[Attribute("", uiwidget: UIWidgets.Object,
		desc: "Collection of radio channel plans")]
	ref array<ref OETA_RadioPlanConfig> Plans;
}
