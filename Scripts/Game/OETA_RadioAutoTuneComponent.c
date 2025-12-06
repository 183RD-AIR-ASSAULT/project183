// -----------------------------------------------------------------------------
// OETA_RadioAutoTuneComponent.c (Reforger 1.6, player-based channel text)
// - Channel 0 -> Player's group frequency (via SCR_GroupsManagerComponent / SCR_AIGroup)
// - Channel 1 -> Faction frequency
// - Channels 3–8 -> Optional plan, based on Channel 0 frequency
// - Channel text stored in OETA_RadioChannelNamesComponent on the same
//   entity as this component (player-based, not radio-prefab-based).
// -----------------------------------------------------------------------------

// --- Channel text component (for UI) ----------------------------------------
class OETA_RadioChannelNamesComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Holds human-readable text per radio channel")]
class OETA_RadioChannelNamesComponent : ScriptComponent
{
	protected ref map<int, string> m_ChannelText;

	protected void EnsureMap()
	{
		if (!m_ChannelText)
			m_ChannelText = new map<int, string>();
	}

	// Set text for a channel
	void SetChannelText(int index, string text)
	{
		EnsureMap();

		if (text == "")
		{
			m_ChannelText.Remove(index);
			return;
		}

		m_ChannelText.Set(index, text);
	}

	// Get text for a channel
	string GetChannelText(int index)
	{
		if (!m_ChannelText)
			return "";

		string outText;
		if (m_ChannelText.Find(index, outText))
			return outText;

		return "";
	}

	// Remove text from channels in a block
	void ClearTextRange(int startIndex, int count)
	{
		if (!m_ChannelText)
			return;

		for (int i = 0; i < count; i++)
		{
			int idx = startIndex + i;
			m_ChannelText.Remove(idx);
		}
	}

	// Build UI label for the channel:
	// Example output: "3: 45250 kHz (TankCmd)"
	string FormatChannelText(BaseRadioComponent radio, int channelIndex)
	{
		if (!radio)
			return "";

		int transCount = radio.TransceiversCount();
		if (channelIndex < 0 || channelIndex >= transCount)
			return "";

		BaseTransceiver trans = radio.GetTransceiver(channelIndex);
		if (!trans)
			return "";

		int freq = trans.GetFrequency();
		string suffix = "";

		string txt = GetChannelText(channelIndex);
		if (txt != "")
			suffix = string.Format(" (%1)", txt);

		return string.Format("%1: %2 kHz%3", channelIndex + 1, freq, suffix);
	}
}

// --- Channel plan support ----------------------------------------------------
class OETA_RadioChannelEntry
{
	int    m_FrequencyKHz;
	string m_Text;

	void OETA_RadioChannelEntry(int freqKHz, string text)
	{
		m_FrequencyKHz = freqKHz;
		m_Text         = text;
	}
};

class OETA_RadioChannelPlan
{
	int m_BaseFrequencyKHz;
	ref array<ref OETA_RadioChannelEntry> m_Entries;

	void OETA_RadioChannelPlan(int baseFreqKHz)
	{
		m_BaseFrequencyKHz = baseFreqKHz;
		m_Entries          = new array<ref OETA_RadioChannelEntry>();
	}

	// Normal entry: may set freq and/or text
	void Add(int freqKHz, string text)
	{
		m_Entries.Insert(new OETA_RadioChannelEntry(freqKHz, text));
	}

	// Pure null slot: leaves that channel untouched (no freq, no text change)
	void AddEmpty()
	{
		m_Entries.Insert(new OETA_RadioChannelEntry(0, ""));
	}

	bool Matches(int baseFreqKHz)
	{
		return (baseFreqKHz == m_BaseFrequencyKHz);
	}

	void ApplyToRadio(BaseRadioComponent radio, OETA_RadioChannelNamesComponent namesComp, int startChannelIndex, bool debugLog)
	{
		if (!radio)
			return;

		int transCount = radio.TransceiversCount();
		int numEntries = m_Entries.Count();

		for (int i = 0; i < numEntries; i++)
		{
			int chIndex = startChannelIndex + i;
			if (chIndex >= transCount)
				break;

			OETA_RadioChannelEntry entry = m_Entries[i];
			if (!entry)
				continue;

			bool hasFreq = entry.m_FrequencyKHz > 0;
			bool hasText = entry.m_Text != "";

			// Pure null slot: leave channel as-is
			if (!hasFreq && !hasText)
				continue;

			BaseTransceiver ch = radio.GetTransceiver(chIndex);
			if (!ch)
				continue;

			// Frequency change (if provided)
			if (hasFreq)
			{
				int freq = entry.m_FrequencyKHz;
				int fMin = ch.GetMinFrequency();
				int fMax = ch.GetMaxFrequency();

				if (freq < fMin) freq = fMin;
				if (freq > fMax) freq = fMax;

				ch.SetFrequency(freq);

				if (debugLog)
					PrintFormat("[OETA_RadioAutoTune] Plan ch%1 -> %2 kHz (%3)", chIndex, freq, entry.m_Text);
			}

			// UI text change (optionally independent of frequency)
			if (namesComp && hasText)
			{
				namesComp.SetChannelText(chIndex, entry.m_Text);
			}
		}
	}
};

// --- Group fallback settings component --------------------------------------
class OETA_GroupRadioSettingsComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Holds group radio settings (fallback)")]
class OETA_GroupRadioSettingsComponent : ScriptComponent
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Group Frequency (kHz). 0 = unset")]
	protected int m_GroupFrequencyKHz;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Group Encryption Key (optional)")]
	protected string m_GroupEncryptionKey;

	int  GetGroupFrequencyKHz() { return m_GroupFrequencyKHz; }
	bool HasGroupEncryptionKey(out string key) { key = m_GroupEncryptionKey; return key != ""; }
}

// ---- Main auto-tune component ----------------------------------------------
class OETA_RadioAutoTuneComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Auto-tunes radios to group & faction frequencies")]
class OETA_RadioAutoTuneComponent : ScriptComponent
{
	// Which channels to set (zero-based)
	[Attribute(defvalue: "0", uiwidget: UIWidgets.Slider, desc: "Group channel index (Channel 1 = 0)", params: "0 3 1")]
	protected int m_GroupChannelIndex;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Slider, desc: "Faction channel index (Channel 2 = 1)", params: "0 3 1")]
	protected int m_FactionChannelIndex;

	// Channel plan based on Channel 0 (group)
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Apply channel plan to Channels 3–8 based on group (Channel 0) freq")]
	protected bool m_EnableChannelPlans;

	// Start index for plan (Channel 3 = index 2)
	[Attribute(defvalue: "2", uiwidget: UIWidgets.Slider, desc: "Channel plan start index (Channel 3 = 2)", params: "0 7 1")]
	protected int m_ChannelPlanStartIndex;

	// Encryption behavior
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Sync radio encryption key to faction key when faction retunes")]
	protected bool m_SyncFactionEncryption;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Prefer group encryption key (if provided) after group retune")]
	protected bool m_PreferGroupEncryption;

	// Group freq sources
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Group Freq Override (kHz). 0 = disabled")]
	protected int m_GroupFreqOverrideKHz;

	// Debug/testing
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Verbose logging to console (Print)")]
	protected bool m_Debug;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Run on clients (for local testing)")]
	protected bool m_RunOnClient;

	// --- State ---------------------------------------------------------------
	protected IEntity m_Owner;
	protected ScriptedInventoryStorageManagerComponent m_InvMgr;  // 2-arg invoker
	protected InventoryStorageManagerComponent m_InvMgrBase;

	// Faction resolve retry
	protected int m_FactionRetry = 0;
	protected const int MAX_FACTION_RETRY = 20;   // ~4s if 200ms steps

	// Static channel-plan table
	static ref array<ref OETA_RadioChannelPlan> s_Plans;

	// -------------------------------------------------------------------------
	// Plan definition (your maps)
	// -------------------------------------------------------------------------
	static void InitDefaultPlans()
	{
		if (s_Plans)
			return;

		s_Plans = new array<ref OETA_RadioChannelPlan>();

		OETA_RadioChannelPlan air = new OETA_RadioChannelPlan(69500);
		air.Add(70000, "Air2GND");
		air.Add(71000, "TAC1");
		air.Add(72000, "TAC2");
		air.Add(73000, "TAC3");
		air.Add(74000, "TAC4");
		air.Add(75000, "Reserve");
		s_Plans.Insert(air);

		OETA_RadioChannelPlan able = new OETA_RadioChannelPlan(68500);
		able.Add(53000, "JTAC");
		able.Add(54000, "CSB");
		able.Add(70000, "Air2GND");
		able.Add(32000, "Nightmare");
		able.Add(39000, "Lancer");
		able.Add(67000, "Mortar");
		s_Plans.Insert(able);

		OETA_RadioChannelPlan jfire = new OETA_RadioChannelPlan(53000);
		jfire.Add(70000, "AIR2GND");
		jfire.Add(71000, "TAC1");
		jfire.Add(72000, "TAC2");
		jfire.Add(73000, "TAC3");
		jfire.Add(74000, "TAC4");
		jfire.Add(67000, "Mortar");
		s_Plans.Insert(jfire);

		OETA_RadioChannelPlan nightmare = new OETA_RadioChannelPlan(32500);
		nightmare.Add(68000, "COY");
		nightmare.Add(53000, "JTAC");
		nightmare.AddEmpty();
		nightmare.AddEmpty();
		nightmare.AddEmpty();
		nightmare.AddEmpty();
		s_Plans.Insert(nightmare);

		OETA_RadioChannelPlan nightmare1 = new OETA_RadioChannelPlan(33000);
		nightmare1.Add(33500, "A Team");
		nightmare1.Add(34000, "B Team");
		nightmare1.AddEmpty();
		nightmare1.AddEmpty();
		nightmare1.AddEmpty();
		nightmare1.AddEmpty();
		s_Plans.Insert(nightmare1);

		OETA_RadioChannelPlan nightmare2 = new OETA_RadioChannelPlan(34500);
		nightmare2.Add(35000, "A Team");
		nightmare2.Add(35500, "B Team");
		nightmare2.AddEmpty();
		nightmare2.AddEmpty();
		nightmare2.AddEmpty();
		nightmare2.AddEmpty();
		s_Plans.Insert(nightmare2);

		OETA_RadioChannelPlan nightmare3 = new OETA_RadioChannelPlan(36000);
		nightmare3.Add(36500, "A Team");
		nightmare3.Add(37000, "B Team");
		nightmare3.AddEmpty();
		nightmare3.AddEmpty();
		nightmare3.AddEmpty();
		nightmare3.AddEmpty();
		s_Plans.Insert(nightmare3);

		OETA_RadioChannelPlan nightmare4 = new OETA_RadioChannelPlan(37500);
		nightmare4.Add(38000, "A Team");
		nightmare4.Add(38500, "B Team");
		nightmare4.AddEmpty();
		nightmare4.AddEmpty();
		nightmare4.AddEmpty();
		nightmare4.AddEmpty();
		s_Plans.Insert(nightmare4);

		OETA_RadioChannelPlan lancer = new OETA_RadioChannelPlan(39500);
		lancer.Add(68000, "COY");
		lancer.Add(53000, "JTAC");
		lancer.AddEmpty();
		lancer.AddEmpty();
		lancer.AddEmpty();
		lancer.AddEmpty();
		s_Plans.Insert(lancer);

		OETA_RadioChannelPlan lancer1 = new OETA_RadioChannelPlan(40000);
		lancer1.Add(40500, "A Team");
		lancer1.Add(41000, "B Team");
		lancer1.AddEmpty();
		lancer1.AddEmpty();
		lancer1.AddEmpty();
		lancer1.AddEmpty();
		s_Plans.Insert(lancer1);

		OETA_RadioChannelPlan lancer2 = new OETA_RadioChannelPlan(41500);
		lancer2.Add(42000, "A Team");
		lancer2.Add(42500, "B Team");
		lancer2.AddEmpty();
		lancer2.AddEmpty();
		lancer2.AddEmpty();
		lancer2.AddEmpty();
		s_Plans.Insert(lancer2);

		OETA_RadioChannelPlan lancer3 = new OETA_RadioChannelPlan(43000);
		lancer3.Add(43500, "A Team");
		lancer3.Add(44000, "B Team");
		lancer3.AddEmpty();
		lancer3.AddEmpty();
		lancer3.AddEmpty();
		lancer3.AddEmpty();
		s_Plans.Insert(lancer3);

		OETA_RadioChannelPlan lancer4 = new OETA_RadioChannelPlan(44500);
		lancer4.Add(45000, "A Team");
		lancer4.Add(45500, "B Team");
		lancer4.AddEmpty();
		lancer4.AddEmpty();
		lancer4.AddEmpty();
		lancer4.AddEmpty();
		s_Plans.Insert(lancer4);
		
		OETA_RadioChannelPlan trainingc = new OETA_RadioChannelPlan(46500);
		trainingc.Add(68000, "COY");
		trainingc.Add(53000, "JTAC");
		trainingc.AddEmpty();
		trainingc.AddEmpty();
		trainingc.AddEmpty();
		s_Plans.Insert(trainingc);
	}

	// --- Player-based channel text component lookup -------------------------
	protected OETA_RadioChannelNamesComponent FindNamesComponent()
	{
		if (!m_Owner)
			return null;

		// 1) On owner itself (player / entity this component is on)
		OETA_RadioChannelNamesComponent n = OETA_RadioChannelNamesComponent.Cast(
			m_Owner.FindComponent(OETA_RadioChannelNamesComponent)
		);
		if (n) return n;

		// 2) Walk parents
		IEntity p = m_Owner.GetParent();
		while (p)
		{
			n = OETA_RadioChannelNamesComponent.Cast(p.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			p = p.GetParent();
		}

		// 3) Walk children
		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			n = OETA_RadioChannelNamesComponent.Cast(c.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			c = c.GetSibling();
		}

		return null;
	}

	// --- Apply plan ----------------------------------------------------------
	protected void ApplyChannelPlan(BaseRadioComponent radio, int baseFreqKHz)
	{
		if (!m_EnableChannelPlans)
			return;

		InitDefaultPlans();
		if (!s_Plans || s_Plans.Count() == 0)
			return;

		// Player-based text component
		OETA_RadioChannelNamesComponent names = FindNamesComponent();

		// Clear previous text in the plan range (up to 6 channels, 3–8)
		if (names)
			names.ClearTextRange(m_ChannelPlanStartIndex, 6);

		foreach (OETA_RadioChannelPlan plan : s_Plans)
		{
			if (!plan)
				continue;

			if (plan.Matches(baseFreqKHz))
			{
				if (m_Debug)
					PrintFormat("[OETA_RadioAutoTune] Applying plan for base freq %1 kHz", baseFreqKHz);

				plan.ApplyToRadio(radio, names, m_ChannelPlanStartIndex, m_Debug);

				// Optional debug dump of text
				if (m_Debug && names)
				{
					for (int i = 0; i < 6; i++)
					{
						int chIndex = m_ChannelPlanStartIndex + i;
						string t = names.GetChannelText(chIndex);
						PrintFormat("[OETA_RadioAutoTune] Text debug ch%1 = '%2'", chIndex, t);
					}
				}

				return;
			}
		}

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] No matching channel plan for base freq %1 kHz", baseFreqKHz);
	}

	// --- Lifecycle -----------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;

		if (m_Debug) Print("[OETA_RadioAutoTune] OnPostInit");

		if (!Replication.IsServer() && !m_RunOnClient)
		{
			if (m_Debug) Print("[OETA_RadioAutoTune] Not server; skipping (enable 'Run on clients' to test)");
			return;
		}

		InitDefaultPlans();
		LocateInvMgr();
		GetGame().GetCallqueue().CallLater(DeferredHook, 50, false);
	}

	override protected void OnDelete(IEntity owner)
	{
		if (m_InvMgr && m_InvMgr.m_OnItemAddedInvoker)
			m_InvMgr.m_OnItemAddedInvoker.Remove(OnItemAdded);

		super.OnDelete(owner);
	}

	protected void DeferredHook()
	{
		if (m_Debug) Print("[OETA_RadioAutoTune] DeferredHook");

		LocateInvMgr();
		if (m_InvMgr && m_InvMgr.m_OnItemAddedInvoker)
		{
			m_InvMgr.m_OnItemAddedInvoker.Insert(OnItemAdded);
			if (m_Debug) Print("[OETA_RadioAutoTune] Subscribed to 2-arg m_OnItemAddedInvoker");
		}
		else if (m_Debug) Print("[OETA_RadioAutoTune] Inventory manager / invoker not found");

		WaitForFactionThenRetune();
	}

	// --- Inventory hookup ----------------------------------------------------
	protected void LocateInvMgrOn(IEntity ent)
	{
		if (!ent) return;
		if (!m_InvMgr) m_InvMgr = ScriptedInventoryStorageManagerComponent.Cast(
			ent.FindComponent(ScriptedInventoryStorageManagerComponent)
		);
	}

	protected void ScanChildrenForInvMgr(IEntity ent)
	{
		IEntity c = ent.GetChildren();
		while (!m_InvMgr && c)
		{
			LocateInvMgrOn(c);
			c = c.GetSibling();
		}
	}

	protected void LocateInvMgr()
	{
		LocateInvMgrOn(m_Owner);
		if (!m_InvMgr) ScanChildrenForInvMgr(m_Owner);

		m_InvMgrBase = InventoryStorageManagerComponent.Cast(m_InvMgr);

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] LocateInvMgr -> inv=%1 base=%2", m_InvMgr, m_InvMgrBase);
	}

	// --- Invoker handler (2 args, Reforger 1.6) -----------------------------
	void OnItemAdded(IEntity item, BaseInventoryStorageComponent storage)
	{
		if (m_Debug) PrintFormat("[OETA_RadioAutoTune] OnItemAdded item=%1 storage=%2", item, storage);

		if (!item) return;
		BaseRadioComponent radio = BaseRadioComponent.Cast(item.FindComponent(BaseRadioComponent));
		if (radio) RetuneRadioBoth(radio);
	}

	// --- Retune flow ---------------------------------------------------------
	protected void WaitForFactionThenRetune()
	{
		SCR_Faction fac = ResolveFactionOnce();
		if (fac)
		{
			if (m_Debug) PrintFormat("[OETA_RadioAutoTune] Faction resolved: %1", fac);
			RetuneAllRadios();
			return;
		}

		if (m_FactionRetry < MAX_FACTION_RETRY)
		{
			m_FactionRetry++;
			if (m_Debug) PrintFormat("[OETA_RadioAutoTune] ResolveFaction failed (try %1/%2) — retrying...",
				m_FactionRetry, MAX_FACTION_RETRY);
			GetGame().GetCallqueue().CallLater(WaitForFactionThenRetune, 200, false);
		}
		else if (m_Debug) Print("[OETA_RadioAutoTune] ResolveFaction failed — giving up");
	}

	protected void RetuneAllRadios()
	{
		if (!m_InvMgrBase) return;

		array<IEntity> withComps = {};
		array<typename> want = { BaseRadioComponent };
		m_InvMgrBase.FindItemsWithComponents(withComps, want);

		if (m_Debug) PrintFormat("[OETA_RadioAutoTune] RetuneAllRadios count=%1", withComps.Count());

		foreach (IEntity it : withComps)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(it.FindComponent(BaseRadioComponent));
			if (radio) RetuneRadioBoth(radio);
		}
	}

	// --- Core retune for a single radio -------------------------------------
	protected void RetuneRadioBoth(BaseRadioComponent radio)
	{
		if (!radio) return;

		// Player-based text component
		OETA_RadioChannelNamesComponent names = FindNamesComponent();
		int groupFreqApplied = 0;

		// --- GROUP on channel m_GroupChannelIndex ----------------------------
		if (radio.TransceiversCount() > m_GroupChannelIndex)
		{
			BaseTransceiver chG = radio.GetTransceiver(m_GroupChannelIndex);
			if (chG)
			{
				int gFreq; string gKey;
				if (ResolveGroupRadio(gFreq, gKey))
				{
					int gMin = chG.GetMinFrequency();
					int gMax = chG.GetMaxFrequency();
					if (gFreq < gMin) gFreq = gMin;
					if (gFreq > gMax) gFreq = gMax;

					chG.SetFrequency(gFreq);
					groupFreqApplied = gFreq;

					if (m_PreferGroupEncryption && gKey != "")
						radio.SetEncryptionKey(gKey);

					// UI text
					if (names)
						names.SetChannelText(m_GroupChannelIndex, "Group");

					if (m_Debug) PrintFormat("[OETA_RadioAutoTune] Group ch%1 -> %2 kHz",
						m_GroupChannelIndex, gFreq);
				}
				else if (m_Debug) Print("[OETA_RadioAutoTune] No group frequency found");
			}
		}
		else if (m_Debug)
		{
			PrintFormat("[OETA_RadioAutoTune] Radio has %1 transceivers; group index %2 invalid",
				radio.TransceiversCount(), m_GroupChannelIndex);
		}

		// --- Plan based on group channel (Channel 0) -------------------------
		if (groupFreqApplied > 0)
		{
			ApplyChannelPlan(radio, groupFreqApplied);
		}

		// --- FACTION on channel m_FactionChannelIndex ------------------------
		if (radio.TransceiversCount() > m_FactionChannelIndex)
		{
			BaseTransceiver chF = radio.GetTransceiver(m_FactionChannelIndex);
			if (chF)
			{
				SCR_Faction fac = ResolveFactionOnce();
				if (fac)
				{
					int fFreq = fac.GetFactionRadioFrequency();
					int fMin = chF.GetMinFrequency();
					int fMax = chF.GetMaxFrequency();
					if (fFreq < fMin) fFreq = fMin;
					if (fFreq > fMax) fFreq = fMax;

					chF.SetFrequency(fFreq);
					if (m_SyncFactionEncryption)
						radio.SetEncryptionKey(fac.GetFactionRadioEncryptionKey());

					// UI text
					if (names)
						names.SetChannelText(m_FactionChannelIndex, "Faction");

					if (m_Debug) PrintFormat("[OETA_RadioAutoTune] Faction ch%1 -> %2 kHz",
						m_FactionChannelIndex, fFreq);
				}
				else if (m_Debug) Print("[OETA_RadioAutoTune] ResolveFaction failed inside RetuneRadioBoth");
			}
		}
		else if (m_Debug)
		{
			PrintFormat("[OETA_RadioAutoTune] Radio has %1 transceivers; faction index %2 invalid",
				radio.TransceiversCount(), m_FactionChannelIndex);
		}
	}

	// --- Group frequency resolution -----------------------------------------
	// Order: explicit override -> player's live group -> fallback settings component
	protected bool ResolveGroupRadio(out int freqKHz, out string key)
	{
		// 1) Explicit override
		if (m_GroupFreqOverrideKHz > 0)
		{
			freqKHz = m_GroupFreqOverrideKHz; key = ""; return true;
		}

		// 2) Player's current group via GroupsManager (preferred)
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			int pid = pm.GetPlayerIdFromControlledEntity(m_Owner);
			if (pid != 0)
			{
				SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
				if (gm)
				{
					SCR_AIGroup group = gm.GetPlayerGroup(pid);
					if (group)
					{
						int liveFreq = group.GetRadioFrequency(); // kHz
						if (m_Debug) PrintFormat("[OETA_RadioAutoTune] Group from GM: id=%1 freq=%2",
							group.GetGroupID(), liveFreq);
						if (liveFreq > 0) { freqKHz = liveFreq; key = ""; return true; }
					}
				}
			}
		}

		// 3) Fallback: a nearby OETA_GroupRadioSettingsComponent
		OETA_GroupRadioSettingsComponent g = FindGroupSettings();
		if (g && g.GetGroupFrequencyKHz() > 0)
		{
			freqKHz = g.GetGroupFrequencyKHz();
			if (!g.HasGroupEncryptionKey(key)) key = "";
			return true;
		}

		// Nothing found
		freqKHz = 0; key = ""; return false;
	}

	protected OETA_GroupRadioSettingsComponent FindGroupSettings()
	{
		OETA_GroupRadioSettingsComponent g = OETA_GroupRadioSettingsComponent.Cast(
			m_Owner.FindComponent(OETA_GroupRadioSettingsComponent)
		);
		if (g) return g;

		IEntity p = m_Owner.GetParent();
		while (p)
		{
			g = OETA_GroupRadioSettingsComponent.Cast(p.FindComponent(OETA_GroupRadioSettingsComponent));
			if (g) return g;
			p = p.GetParent();
		}

		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			g = OETA_GroupRadioSettingsComponent.Cast(c.FindComponent(OETA_GroupRadioSettingsComponent));
			if (g) return g;
			c = c.GetSibling();
		}
		return null;
	}

	// --- Faction helpers -----------------------------------------------------
	protected SCR_FactionAffiliationComponent FindAffiliationComp()
	{
		SCR_FactionAffiliationComponent aff = SCR_FactionAffiliationComponent.Cast(
			m_Owner.FindComponent(SCR_FactionAffiliationComponent)
		);
		if (aff) return aff;

		IEntity p = m_Owner.GetParent();
		while (p)
		{
			aff = SCR_FactionAffiliationComponent.Cast(p.FindComponent(SCR_FactionAffiliationComponent));
			if (aff) return aff;
			p = p.GetParent();
		}

		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			aff = SCR_FactionAffiliationComponent.Cast(c.FindComponent(SCR_FactionAffiliationComponent));
			if (aff) return aff;
			c = c.GetSibling();
		}

		return null;
	}

	protected SCR_Faction ResolveFactionOnce()
	{
		SCR_FactionAffiliationComponent aff = FindAffiliationComp();
		if (aff)
		{
			Faction f = aff.GetAffiliatedFaction();
			if (f)
			{
				SCR_Faction sf = SCR_Faction.Cast(f);
				if (sf) return sf;
			}
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			int pid = pm.GetPlayerIdFromControlledEntity(m_Owner);
			if (pid != 0)
			{
				SCR_Faction pf = SCR_Faction.Cast(SCR_FactionManager.SGetPlayerFaction(pid));
				if (pf) return pf;
			}
		}
		return null;
	}

	// --- Utility -------------------------------------------------------------
	void ForceRetune()
	{
		if (!Replication.IsServer() && !m_RunOnClient) return;
		if (m_Debug) Print("[OETA_RadioAutoTune] ForceRetune");
		RetuneAllRadios();
	}
}
