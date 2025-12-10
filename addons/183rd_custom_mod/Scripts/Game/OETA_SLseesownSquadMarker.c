modded class SCR_MapMarkerSquadLeader
{
	override void OnPlayerIdUpdate()
	{
		// Get local player controller
		PlayerController pController = GetGame().GetPlayerController();
		if (!pController)
			return;

		int localPlayerId = pController.GetPlayerId();

		// If this marker belongs to us, don't display it locally
		if (m_PlayerID == localPlayerId)
		{
			SetLocalVisible(false); // hide our own squad leader marker
		}
		else
		{
			SetLocalVisible(true);  // show other squad leaders
		}
	}
}
