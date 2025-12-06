// -----------------------------------------------------------------------------
// OETA_RadioChannelListUIComponent.c
// Helper for radio UI: builds "N: freq kHz (Name)" labels using
// OETA_RadioChannelNamesComponent.FormatChannelText.
// -----------------------------------------------------------------------------

class OETA_RadioChannelListUIComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Helper for radio channel list UI formatting")]
class OETA_RadioChannelListUIComponent : ScriptComponent
{
	// Find the nearest OETA_RadioChannelNamesComponent given a radio
	static OETA_RadioChannelNamesComponent FindNamesForRadio(BaseRadioComponent radio)
	{
		if (!radio)
			return null;

		IEntity owner = radio.GetOwner();
		if (!owner)
			return null;

		// 1) On owner
		OETA_RadioChannelNamesComponent n = OETA_RadioChannelNamesComponent.Cast(
			owner.FindComponent(OETA_RadioChannelNamesComponent)
		);
		if (n) return n;

		// 2) Parents
		IEntity p = owner.GetParent();
		while (p)
		{
			n = OETA_RadioChannelNamesComponent.Cast(p.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			p = p.GetParent();
		}

		// 3) Children
		IEntity c = owner.GetChildren();
		while (c)
		{
			n = OETA_RadioChannelNamesComponent.Cast(c.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			c = c.GetSibling();
		}

		return null;
	}

	// Static helper you can call from a UI binding:
	// Example use in script: OETA_RadioChannelListUIComponent.BuildLabel(radio, index)
	static string BuildLabel(BaseRadioComponent radio, int channelIndex)
	{
		if (!radio)
			return "";

		OETA_RadioChannelNamesComponent names = FindNamesForRadio(radio);
		if (!names)
		{
			// Fallback: vanilla-style "N: freq kHz"
			int transCount = radio.TransceiversCount();
			if (channelIndex < 0 || channelIndex >= transCount)
				return "";

			BaseTransceiver trans = radio.GetTransceiver(channelIndex);
			if (!trans)
				return "";

			int freq = trans.GetFrequency();
			return string.Format("%1: %2 kHz", channelIndex + 1, freq);
		}

		return names.FormatChannelText(radio, channelIndex);
	}
}
