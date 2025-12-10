// -----------------------------------------------------------------------------
// OETA_RadioPlanOverrideComponent.c
// Per-radio override: assign a specific channel plan to a radio owner
// (vehicle, backpack, static set, etc).
//
// - PlanName must match OETA_RadioPlanConfig.Name in the .conf
// - StartChannelIndexOverride:
//      -1 = use default (index 0, Channel 1)
//       n = start at that zero-based index
// -----------------------------------------------------------------------------

class OETA_RadioPlanOverrideComponentClass : ScriptComponentClass {}

[ComponentEditorProps(
	category: "Radio",
	description: "Assigns a specific radio channel plan to this radio"
)]
class OETA_RadioPlanOverrideComponent : ScriptComponent
{
	[Attribute(defvalue: "",
		uiwidget: UIWidgets.EditBox,
		desc: "Plan name (must match Name in OETA_RadioPlanConfig)")]
	protected string m_PlanName;

	// -1 = default start index (0 -> Channel 1)
	[Attribute(defvalue: "-1",
		uiwidget: UIWidgets.Slider,
		desc: "Start channel index override (-1 = default, usually channel 1)",
		params: "-1 15 1")]
	protected int m_StartChannelIndexOverride;

	string GetPlanName()
	{
		return m_PlanName;
	}

	int GetStartChannelIndexOverride()
	{
		return m_StartChannelIndexOverride;
	}
}
