//------------------------------------------------------------------------------------------------
// Advanced Fast-Travel System with multiple destinations, cooldown, and random selection.
// Fully configurable through Workbench attributes.
//
// NOTE:
// This ScriptedUserAction is designed to run its EFFECTS on the SERVER.
// HasLocalEffectOnlyScript() returns false, so the action system automatically
// reroutes PerformAction() to the server instance.
//------------------------------------------------------------------------------------------------
class OETA_FastTravelActionManager : ScriptedUserAction
{
	// Minimum offset from destination to avoid spawning directly inside the location
	protected const float TELEPORT_OFFSET = 1.0;
	
	// Minimum distance required between player and destination to allow teleport
	protected const float MIN_DISTANCE_FOR_TELEPORT = 1.5;

	// List of destination entity names
	[Attribute("", UIWidgets.Object, "Destination List", "World entity names that can be used as destinations")]
	protected ref array<string> m_FastTravel_Destinations;

	// Cooldown time in seconds for this action
	[Attribute("10", UIWidgets.Slider, "Cooldown (seconds)", "Wait time between teleports per player", params: "0 300 1")]
	protected int m_Cooldown;

	// Cooldown map: PlayerID → is in cooldown?
	protected ref map<int, bool> m_PlayerCooldowns;
	
	// Timestamp map: PlayerID → last used (milliseconds)
	protected ref map<int, float> m_PlayerLastUse;

	//------------------------------------------------------------------------------------------------
	// Constructor - Setup lists and cooldown tables
	void OETA_FastTravelActionManager()
	{
		m_PlayerCooldowns = new map<int, bool>();
		m_PlayerLastUse = new map<int, float>();
		
		if (!m_FastTravel_Destinations)
			m_FastTravel_Destinations = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	// Main action execution called when player presses the Use (F) key
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// IMPORTANT:
		// DO NOT exit if !Replication.IsServer().
		// Because HasLocalEffectOnlyScript() = false,
		// the engine automatically routes this call to the SERVER instance.

		if (!ValidateEntities(pOwnerEntity, pUserEntity))
			return;

		PlayerManager pMan = GetGame().GetPlayerManager();
		if (!pMan)
			return;

		int playerId = pMan.GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId == 0)
			return;

		// Check cooldown
		if (IsPlayerOnCooldown(playerId))
		{
			float remaining = GetRemainingCooldown(playerId);
			ShowHintToPlayer(pUserEntity, string.Format("Wait %1 seconds", Math.Ceil(remaining)));
			return;
		}

		// Pick a random destination
		string destName = SelectRandomDestination();
		if (destName.IsEmpty())
			return;

		string originName = pOwnerEntity.GetName();

		// Perform teleport
		if (TeleportPlayer(pUserEntity, destName, originName))
		{
			StartCooldown(playerId);
			ShowHintToPlayer(pUserEntity, string.Format("Traveling to: %1", destName));
		}
	}

	//------------------------------------------------------------------------------------------------
	// Ensure both entities exist
	protected bool ValidateEntities(IEntity owner, IEntity user)
	{
		return owner && user;
	}

	//------------------------------------------------------------------------------------------------
	// Checks whether player is currently in cooldown
	protected bool IsPlayerOnCooldown(int playerId)
	{
		return m_PlayerCooldowns.Contains(playerId) && m_PlayerCooldowns.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Random destination picker
	protected string SelectRandomDestination()
	{
		if (!m_FastTravel_Destinations || m_FastTravel_Destinations.IsEmpty())
			return "";

		int idx = Math.RandomInt(0, m_FastTravel_Destinations.Count());
		return m_FastTravel_Destinations.Get(idx);
	}

	//------------------------------------------------------------------------------------------------
	// Teleport the player to the destination entity
	protected bool TeleportPlayer(IEntity player, string destinationName, string originName)
	{
		World world = GetGame().GetWorld();
		if (!world || !player)
			return false;

		IEntity dest = world.FindEntityByName(destinationName);
		if (!dest)
			return false;

		vector playerPos = player.GetOrigin();
		vector destPos = dest.GetOrigin();
		float dist = vector.Distance(playerPos, destPos);

		// Prevent teleporting when already at destination
		if (dist < MIN_DISTANCE_FOR_TELEPORT)
			return false;

		// Compute a small offset direction to avoid placing player inside the entity
		vector offsetDir = (playerPos - destPos).Normalized();
		if (offsetDir.LengthSq() <= 0.0)
			offsetDir = "1 0 0"; // Fallback

		vector finalPos = destPos + (offsetDir * TELEPORT_OFFSET);

		// SERVER-SIDE authoritative teleport
		player.SetOrigin(finalPos);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Start cooldown for this player
	protected void StartCooldown(int playerId)
	{
		if (m_Cooldown <= 0)
			return;

		m_PlayerCooldowns.Set(playerId, true);

		World world = GetGame().GetWorld();
		if (!world)
			return;

		m_PlayerLastUse.Set(playerId, world.GetWorldTime());

		// Schedule cooldown reset
		GetGame().GetCallqueue().CallLater(ResetCooldown, m_Cooldown * 1000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Reset player cooldown
	protected void ResetCooldown(int playerId)
	{
		m_PlayerCooldowns.Set(playerId, false);
	}

	//------------------------------------------------------------------------------------------------
	// Compute remaining cooldown time
	protected float GetRemainingCooldown(int playerId)
	{
		if (!m_PlayerLastUse || !m_PlayerLastUse.Contains(playerId))
			return 0;

		World world = GetGame().GetWorld();
		if (!world)
			return 0;

		float lastUse = m_PlayerLastUse.Get(playerId);
		float now = world.GetWorldTime();
		float elapsed = (now - lastUse) / 1000.0;

		float remaining = m_Cooldown - elapsed;
		return Math.Max(remaining, 0);
	}

	//------------------------------------------------------------------------------------------------
	// Show a hint to the player (client-side UI)
	protected void ShowHintToPlayer(IEntity player, string message)
	{
		if (!player || message.IsEmpty())
			return;

		SCR_HintManagerComponent.ShowCustomHint(message, "Fast Travel", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	// Always allow the action to be shown
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Always allow the action to be performed
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Action effects are networked → must run on the server
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}
