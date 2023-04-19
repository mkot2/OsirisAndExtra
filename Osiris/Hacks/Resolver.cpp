#include "AimbotFunctions.h"
#include "Animations.h"
#include "resolver.h"

#include "../SDK/UserCmd.h"
#include "../Logger.h"

#include "../SDK/GameEvent.h"
#include "../GameData.h"

std::deque<resolver::snap_shot> snapshots;

float desync_angle{ 0 };
UserCmd* command;

void resolver::reset() noexcept
{
	snapshots.clear();
}

void resolver::save_record(const int player_index, const float player_simulation_time) noexcept
{
	const auto entity = interfaces->entityList->getEntity(player_index);
	const auto player = Animations::getPlayer(player_index);
	if (!player.gotMatrix || !entity)
		return;

	snap_shot snapshot;
	snapshot.player = player;
	snapshot.player_index = player_index;
	snapshot.eye_position = localPlayer->getEyePosition();
	snapshot.model = entity->getModel();

	if (player.simulationTime >= player_simulation_time - 0.001f && player.simulationTime <= player_simulation_time + 0.001f) {
		snapshots.push_back(snapshot);
		return;
	}

	for (int i = 0; i < static_cast<int>(player.backtrackRecords.size()); i++) {
		if (player.backtrackRecords.at(i).simulationTime >= player_simulation_time - 0.001f && player.backtrackRecords.at(i).simulationTime <= player_simulation_time + 0.001f) {
			snapshot.backtrack_record = i;
			snapshots.push_back(snapshot);
			return;
		}
	}
}

void resolver::get_event(GameEvent* event) noexcept
{
	if (!localPlayer)
		return;
	const auto active_weapon = localPlayer->getActiveWeapon();
	if (!active_weapon || !active_weapon->clip())
		return;
	if (!event || !localPlayer || interfaces->engine->isHLTV())
		return;

	switch (fnv::hashRuntime(event->getName())) {
	case fnv::hash("round_start"):
	{
		//Reset all
		const auto players = Animations::setPlayers();
		if (players->empty())
			break;

		for (auto& player : *players) {
			player.misses = 0;
		}
		snapshots.clear();
		desync_angle = 0;
		break;
	}
	case fnv::hash("weapon_fire"):
	{
		//Reset player
		if (snapshots.empty())
			break;
		const auto playerId = event->getInt(xorstr_("userid"));
		if (playerId == localPlayer->getUserId())
			break;

		const auto index = interfaces->engine->getPlayerForUserID(playerId);
		Animations::setPlayer(index)->shot = true;
		break;
	}
	case fnv::hash("player_hurt"):
	{
		if (snapshots.empty())
			break;
		if (!localPlayer || !localPlayer->isAlive()) {
			snapshots.clear();
			return;
		}

		if (event->getInt(xorstr_("attacker")) != localPlayer->getUserId())
			break;

		const auto hitgroup = event->getInt(xorstr_("hitgroup"));
		if (hitgroup < HitGroup::Head || hitgroup > HitGroup::RightLeg)
			break;
		const auto index{ interfaces->engine->getPlayerForUserID(event->getInt(xorstr_("userid"))) };
		const auto& [player, model, eyePosition, bulletImpact, gotImpact, time, playerIndex, backtrackRecord] = snapshots.front();
		if (desync_angle != 0.f && hitgroup == HitGroup::Head)
			Animations::setPlayer(index)->workingangle = desync_angle;
		const auto entity = interfaces->entityList->getEntity(playerIndex);
		// I am deeply sorry for such a disgusting line code
		Logger::addLog(std::vformat(std::string_view(xorstr_("Resolver: Hit {}{}, Hitgroup {}, Desync Angle: {}")), std::make_format_args(entity->getPlayerName(), backtrackRecord > 0 ? std::vformat(std::string_view(xorstr_(", BT[{}]")), std::make_format_args(backtrackRecord)) : xorstr_(""), HitGroup::hitgroup_text[hitgroup], desync_angle)));
		++hits;
		hit_rate = misses != 0 ? hits / (static_cast<double>(hits) + misses) * 100. : 100.;
		if (!entity->isAlive())
			desync_angle = 0.f;
		snapshots.pop_front(); //Hit somebody so don't calculate
		break;
	}
	case fnv::hash("bullet_impact"):
	{
		if (snapshots.empty())
			break;

		auto& [player, model, eyePosition, bulletImpact, gotImpact, time, playerIndex, backtrackRecord] = snapshots.front();
		if (event->getInt(xorstr_("userid")) == localPlayer->getUserId() && !gotImpact) {
			time = memory->globalVars->serverTime();
			bulletImpact = Vector{ event->getFloat(xorstr_("x")), event->getFloat(xorstr_("y")), event->getFloat(xorstr_("z")) };
			gotImpact = true;
		}
		if (player.shot)
			anti_one_tap(event->getInt(xorstr_("userid")), interfaces->entityList->getEntity(playerIndex), Vector{ event->getFloat(xorstr_("x")),event->getFloat(xorstr_("y")),event->getFloat(xorstr_("z")) });
		break;
	}
	default:
		break;
	}

	if (!config->ragebot[getWeaponIndex(active_weapon->itemDefinitionIndex2())].resolver)
		snapshots.clear();
}

void resolver::process_missed_shots() noexcept
{
	if (!localPlayer)
		return;
	const auto active_weapon = localPlayer->getActiveWeapon();
	if (!active_weapon || !active_weapon->clip())
		return;
	if (!config->ragebot[getWeaponIndex(active_weapon->itemDefinitionIndex2())].resolver) {
		snapshots.clear();
		return;
	}

	if (!localPlayer || !localPlayer->isAlive()) {
		snapshots.clear();
		return;
	}

	if (snapshots.empty())
		return;

	if (const auto& [player, model, eye_position, bullet_impact, got_impact, time, player_index, backtrack_record] = snapshots.front(); time >= -1.001f && time <= -0.999f) // Didn't get data yet
		return;

	auto& [player, s_model, eyePosition, bulletImpact, gotImpact, time, playerIndex, backtrackRecord] = snapshots.front();
	snapshots.pop_front(); //got the info no need for this
	if (!player.gotMatrix)
		return;

	const auto entity = interfaces->entityList->getEntity(playerIndex);
	if (!entity)
		return;

	const Model* model = s_model;
	if (!model)
		return;

	StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
	if (!hdr)
		return;

	StudioHitboxSet* set = hdr->getHitboxSet(0);
	if (!set)
		return;

	const auto angle = AimbotFunction::calculateRelativeAngle(eyePosition, bulletImpact, Vector{ });
	const auto end = bulletImpact + Vector::fromAngle(angle) * 2000.f;

	const matrix3x4* matrix = backtrackRecord == -1 || player.backtrackRecords.size() < static_cast<unsigned int>(
			backtrackRecord) - 1
			? player.matrix.data()
			: player.backtrackRecords.at(backtrackRecord).matrix;

	bool resolver_missed = false;

	for (int hitbox = 0; hitbox < Max; hitbox++)
		if (AimbotFunction::hitboxIntersection(matrix, hitbox, set, eyePosition, end)) {
			resolver_missed = true;
			if (desync_angle >= player.workingangle - 0.001f && desync_angle <= player.workingangle + 0.001f && desync_angle != 0.f)
				player.workingangle = 0.f;
			Animations::setPlayer(playerIndex)->misses++;
			if (!std::ranges::count(player.blacklisted, desync_angle))
				player.blacklisted.push_back(desync_angle);
			// I am sorry for this too
			Logger::addLog(std::vformat(std::string_view(xorstr_("Resolver: Missed {} shots to {} due to resolver{}, Hitbox: {}, Desync Angle: {}")), std::make_format_args(player.misses + 1, entity->getPlayerName(), backtrackRecord > 0 ? std::vformat(std::string_view(xorstr_(", BT[{}]")), std::make_format_args(backtrackRecord)) : xorstr_(""), hitbox_text[hitbox], desync_angle)));
			++misses;
			hit_rate = hits / (static_cast<double>(hits) + misses) * 100.;
			desync_angle = 0;
			break;
		}
	if (!resolver_missed) {
		// And this too
		Logger::addLog(std::vformat(std::string_view(xorstr_("Resolver: Missed {} due to spread")), std::make_format_args(entity->getPlayerName())));
		++misses;
		hit_rate = hits / (static_cast<double>(hits) + misses) * 100.;
	}
}

float get_backward_side(Entity* entity)
{
	if (!entity->isAlive())
		return -1.f;
	const float result = Helpers::angleDiff(localPlayer->origin().y, entity->origin().y);
	return result;
}
float get_angle(Entity* entity)
{
	return Helpers::angleNormalize(entity->eyeAngles().y);
}
float get_forward_yaw(Entity* entity)
{
	return Helpers::angleNormalize(get_backward_side(entity) - 180.f);
}

void resolver::run_pre_update(Animations::Players& player, Entity* entity) noexcept
{
	if (!localPlayer)
		return;
	const auto active_weapon = localPlayer->getActiveWeapon();
	if (!active_weapon || !active_weapon->clip())
		return;
	if (!config->ragebot[getWeaponIndex(active_weapon->itemDefinitionIndex2())].resolver)
		return;

	if (!entity || !entity->isAlive())
		return;

	if (player.chokedPackets <= 0)
		return;
	if (!localPlayer || !localPlayer->isAlive())
		return;
	if (snapshots.empty())
		return;
	auto& [snapshot_player, model, eyePosition, bulletImpact, gotImpact, time, playerIndex, backtrackRecord] = snapshots.front();
	setup_detect(player, entity);
	resolve_entity(player, entity);
	desync_angle = entity->getAnimstate()->footYaw;
	const auto anim_state = entity->getAnimstate();
	anim_state->footYaw = desync_angle;
	if (snapshot_player.workingangle != 0.f && fabs(desync_angle) > fabs(snapshot_player.workingangle)) {
		if (snapshot_player.workingangle < 0.f && player.side == 1)
			snapshot_player.workingangle = fabs(snapshot_player.workingangle);
		else if (snapshot_player.workingangle > 0.f && player.side == -1)
			snapshot_player.workingangle = snapshot_player.workingangle * -1.f;
		desync_angle = snapshot_player.workingangle;
		anim_state->footYaw = desync_angle;
	}
}

void resolver::run_post_update(Animations::Players& player, Entity* entity) noexcept
{
	if (!localPlayer)
		return;
	const auto active_weapon = localPlayer->getActiveWeapon();
	if (!active_weapon || !active_weapon->clip())
		return;
	if (!config->ragebot[getWeaponIndex(active_weapon->itemDefinitionIndex2())].resolver)
		return;

	if (!entity || !entity->isAlive())
		return;

	if (player.chokedPackets <= 0)
		return;
	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (snapshots.empty())
		return;

	auto& [snapshot_player, model, eyePosition, bulletImpact, gotImpact, time, playerIndex, backtrackRecord] = snapshots.front();
	const auto anim_state = entity->getAnimstate();
	setup_detect(player, entity);
	resolve_entity(player, entity);
	desync_angle = anim_state->footYaw;
	if (snapshot_player.workingangle != 0.f && fabs(desync_angle) > fabs(snapshot_player.workingangle)) {
		if (snapshot_player.workingangle < 0.f && player.side == 1)
			snapshot_player.workingangle = fabs(snapshot_player.workingangle);
		else if (snapshot_player.workingangle > 0.f && player.side == -1)
			snapshot_player.workingangle = snapshot_player.workingangle * -1.f;
		desync_angle = snapshot_player.workingangle;
		anim_state->footYaw = desync_angle;
	}
}

float build_server_abs_yaw(Entity* entity, const float angle)
{
	Vector velocity = entity->velocity();
	const auto& anim_state = entity->getAnimstate();
	const float m_fl_eye_yaw = angle;
	float m_fl_goal_feet_yaw = 0.f;

	const float eye_feet_delta = Helpers::angleDiff(m_fl_eye_yaw, m_fl_goal_feet_yaw);

	static auto get_smoothed_velocity = [](const float min_delta, const Vector a, const Vector b) {
		const Vector delta = a - b;
		const float delta_length = delta.length();

		if (delta_length <= min_delta) {
			if (-min_delta <= delta_length)
				return a;
			const float i_radius = 1.0f / (delta_length + FLT_EPSILON);
			return b - delta * i_radius * min_delta;
		}
		const float i_radius = 1.0f / (delta_length + FLT_EPSILON);
		return b + delta * i_radius * min_delta;
	};

	if (const float spd = velocity.squareLength(); spd > std::powf(1.2f * 260.0f, 2.f)) {
		const Vector velocity_normalized = velocity.normalized();
		velocity = velocity_normalized * (1.2f * 260.0f);
	}

	const float m_fl_choked_time = anim_state->lastUpdateTime;
	const float duck_additional = std::clamp(entity->duckAmount() + anim_state->duckAdditional, 0.0f, 1.0f);
	const float duck_amount = anim_state->animDuckAmount;
	const float choked_time = m_fl_choked_time * 6.0f;
	float v28;

	// clamp
	if (duck_additional - duck_amount <= choked_time)
		if (duck_additional - duck_amount >= -choked_time)
			v28 = duck_additional;
		else
			v28 = duck_amount - choked_time;
	else
		v28 = duck_amount + choked_time;

	const float fl_duck_amount = std::clamp(v28, 0.0f, 1.0f);

	const Vector animation_velocity = get_smoothed_velocity(m_fl_choked_time * 2000.0f, velocity, entity->velocity());
	const float speed = std::fminf(animation_velocity.length(), 260.0f);

	float fl_max_movement_speed = 260.0f;

	if (Entity* p_weapon = entity->getActiveWeapon(); p_weapon && p_weapon->getWeaponData())
		fl_max_movement_speed = std::fmaxf(p_weapon->getWeaponData()->maxSpeedAlt, 0.001f);

	float fl_running_speed = speed / (fl_max_movement_speed * 0.520f);

	fl_running_speed = std::clamp(fl_running_speed, 0.0f, 1.0f);

	float fl_yaw_modifier = (anim_state->walkToRunTransition * -0.30000001f - 0.19999999f) * fl_running_speed + 1.0f;

	if (fl_duck_amount > 0.0f) {
		// float fl_ducking_speed = std::clamp(fl_ducking_speed, 0.0f, 1.0f);
		fl_yaw_modifier += fl_duck_amount /* fl_ducking_speed */ * (0.5f - fl_yaw_modifier);
	}

	constexpr float v60 = -58.f;
	constexpr float v61 = 58.f;

	const float fl_min_yaw_modifier = v60 * fl_yaw_modifier;

	if (const float fl_max_yaw_modifier = v61 * fl_yaw_modifier; eye_feet_delta <= fl_max_yaw_modifier) {
		if (fl_min_yaw_modifier > eye_feet_delta)
			m_fl_goal_feet_yaw = fabs(fl_min_yaw_modifier) + m_fl_eye_yaw;
	} else {
		m_fl_goal_feet_yaw = m_fl_eye_yaw - fabs(fl_max_yaw_modifier);
	}

	Helpers::normalizeYaw(m_fl_goal_feet_yaw);

	if (speed > 0.1f || fabs(velocity.z) > 100.0f) {
		m_fl_goal_feet_yaw = Helpers::approachAngle(
			m_fl_eye_yaw,
			m_fl_goal_feet_yaw,
			(anim_state->walkToRunTransition * 20.0f + 30.0f)
			* m_fl_choked_time);
	} else {
		m_fl_goal_feet_yaw = Helpers::approachAngle(
			entity->lby(),
			m_fl_goal_feet_yaw,
			m_fl_choked_time * 100.0f);
	}

	return m_fl_goal_feet_yaw;
}

void resolver::detect_side(Entity* entity, int* side)
{
	/* externals */
	Vector forward{};
	Vector right{};
	Vector up{};
	Trace tr;
	Helpers::angleVectors(Vector(0, get_backward_side(entity), 0), &forward, &right, &up);
	/* filtering */

	const Vector src_3d = entity->getEyePosition();
	const Vector dst_3d = src_3d + forward * 384;

	/* back engine tracers */
	// interfaces->engineTrace->traceRay({ src_3d, dst_3d }, 0x200400B, { entity }, tr);
	// float back_two = (tr.endpos - tr.startpos).length();

	/* right engine tracers */
	interfaces->engineTrace->traceRay(Ray(src_3d + right * 35, dst_3d + right * 35), 0x200400B, { entity }, tr);
	const float right_two = (tr.endpos - tr.startpos).length();

	/* left engine tracers */
	interfaces->engineTrace->traceRay(Ray(src_3d - right * 35, dst_3d - right * 35), 0x200400B, { entity }, tr);

	/* fix side */
	if (const float left_two = (tr.endpos - tr.startpos).length(); left_two > right_two)
		*side = -1;
	else if (right_two > left_two)
		*side = 1;
	else
		*side = 0;
}

void resolver::resolve_entity(const Animations::Players& player, Entity* entity)
{
	// get the players max rotation.
	float max_rotation = entity->getMaxDesyncAngle();
	int index = 0;
	const float eye_yaw = entity->getAnimstate()->eyeYaw;
	if (const bool extended = player.extended; !extended && fabs(max_rotation) > 60.f)
		max_rotation = max_rotation / 1.8f;

	// resolve shooting players separately.
	if (player.shot) {
		entity->getAnimstate()->footYaw = eye_yaw + resolve_shot(player, entity);
		return;
	}
	if (entity->velocity().length2D() <= 0.1f) {
		const float angle_difference = Helpers::angleDiff(eye_yaw, entity->getAnimstate()->footYaw);
		index = 2 * angle_difference <= 0.0f ? 1 : -1;
	} else if (!static_cast<int>(player.layers[12].weight * 1000.f) && entity->velocity().length2D() > 0.1f) {
		const auto m_layer_delta1 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
		const auto m_layer_delta2 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);

		if (const auto m_layer_delta3 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate); m_layer_delta1 < m_layer_delta2
			|| m_layer_delta3 <= m_layer_delta2
			|| static_cast<signed int>((m_layer_delta2 * 1000.0f))) {
			if (m_layer_delta1 >= m_layer_delta3
				&& m_layer_delta2 > m_layer_delta3
				&& !static_cast<signed int>((m_layer_delta3 * 1000.0f)))
				index = 1;
		} else
			index = -1;
	}

	switch (player.misses % 3) {
	case 0: //default
		entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + max_rotation * static_cast<float>(index));
		break;
	case 1: //reverse
		entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y + max_rotation * static_cast<float>(-index));
		break;
	case 2: //middle
		entity->getAnimstate()->footYaw = build_server_abs_yaw(entity, entity->eyeAngles().y);
		break;
	default: break;
	}

}

float resolver::resolve_shot(const Animations::Players& player, Entity* entity)
{
	/* fix unrestricted shot */
	const float fl_pseudo_fire_yaw = Helpers::angleNormalize(Helpers::angleDiff(localPlayer->origin().y, player.matrix[8].origin().y));
	if (player.extended) {
		const float fl_left_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y + 58.f)));
		const float fl_right_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y - 58.f)));

		return fl_left_fire_yaw_delta > fl_right_fire_yaw_delta ? -58.f : 58.f;
	}
	const float fl_left_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y + 28.f)));
	const float fl_right_fire_yaw_delta = fabsf(Helpers::angleNormalize(fl_pseudo_fire_yaw - (entity->eyeAngles().y - 28.f)));

	return fl_left_fire_yaw_delta > fl_right_fire_yaw_delta ? -28.f : 28.f;
}

void resolver::setup_detect(Animations::Players& player, Entity* entity)
{

	// detect if player is using maximum desync.
	if (player.layers[3].cycle == 0.f && player.layers[3].weight == 0.f) {
		player.extended = true;
	}
	/* calling detect side */
	detect_side(entity, &player.side);
	const int side = player.side;
	/* brute-forcing vars */
	float resolve_value = 50.f;
	static float brute = 0.f;
	const float fl_max_rotation = entity->getMaxDesyncAngle();
	const float fl_eye_yaw = entity->getAnimstate()->eyeYaw;
	const bool fl_forward = fabsf(Helpers::angleNormalize(get_angle(entity) - get_forward_yaw(entity))) < 90.f;
	const int fl_shots = player.misses;

	/* clamp angle */
	if (fl_max_rotation < resolve_value) {
		resolve_value = fl_max_rotation;
	}

	/* detect if entity is using max desync angle */
	if (player.extended) {
		resolve_value = fl_max_rotation;
	}

	const float perfect_resolve_yaw = resolve_value;

	/* setup brute-forcing */
	if (fl_shots == 0) {
		brute = perfect_resolve_yaw * static_cast<float>(fl_forward ? -side : side);
	} else {
		switch (fl_shots % 3) {
		case 0:
		{
			brute = perfect_resolve_yaw * static_cast<float>(fl_forward ? -side : side);
		} break;
		case 1:
		{
			brute = perfect_resolve_yaw * static_cast<float>(fl_forward ? side : -side);
		} break;
		case 2:
		{
			brute = 0;
		} break;
		default: break;
		}
	}

	/* fix goal feet yaw */
	entity->getAnimstate()->footYaw = fl_eye_yaw + brute;
}

Vector calc_angle(const Vector source, const Vector entity_pos)
{
	const Vector delta{ source.x - entity_pos.x, source.y - entity_pos.y, source.z - entity_pos.z };
	const auto& [x, y, z] = command->viewangles;
	Vector angles{ Helpers::rad2deg(atan(delta.z / hypot(delta.x, delta.y))) - x, Helpers::rad2deg(atan(delta.y / delta.x)) - y, 0.f };
	if (delta.x >= 0.f)
		angles.y += 180;
	return angles;
}

void resolver::anti_one_tap(const int userid, Entity* entity, const Vector shot)
{
	std::vector<std::reference_wrapper<const PlayerData>> players_ordered{ GameData::players().begin(), GameData::players().end() };
	const auto sorter{
		[](const PlayerData& a, const PlayerData& b) {
			// enemies first
			if (a.enemy != b.enemy)
				return a.enemy && !b.enemy;

			return a.handle < b.handle;
		}
	};
	std::ranges::sort(players_ordered, sorter);
	for (const std::reference_wrapper<const PlayerData>& player : players_ordered) {
		if (player.get().userId == userid && entity->isAlive()) {
			const Vector pos = shot;
			const Vector eye_pos = entity->getEyePosition();
			const auto [x, y, z] = calc_angle(eye_pos, pos);
			const auto [lx, ly, lz] = calc_angle(eye_pos, localPlayer->getEyePosition());
			const Vector delta = { lx - x, ly - y, 0 };
			if (const float fov = sqrt(delta.x * delta.x + delta.y * delta.y); fov < 20.f) {
				Logger::addLog(xorstr_("Resolver: ") + player.get().name + xorstr_(" missed"));
			}
		}
	}
}

void resolver::cmd_grabber(UserCmd* cmd)
{
	command = cmd;
}

void resolver::update_event_listeners(const bool force_remove) noexcept
{
	class impact_event_listener : public GameEventListener {
	public:
		void fireGameEvent(GameEvent* event) override
		{
			get_event(event);
		}
	};

	static std::array<impact_event_listener, 4> listener{};

	if (!localPlayer)
		return;
	const auto active_weapon = localPlayer->getActiveWeapon();
	if (!active_weapon || !active_weapon->clip())
		return;
	const auto weapon_index = getWeaponIndex(active_weapon->itemDefinitionIndex2());
	if (static bool listener_registered = false; config->ragebot[weapon_index].resolver && !listener_registered) {
		interfaces->gameEventManager->addListener(listener.data(), xorstr_("bullet_impact"));
		interfaces->gameEventManager->addListener(&listener[1], xorstr_("player_hurt"));
		interfaces->gameEventManager->addListener(&listener[2], xorstr_("round_start"));
		interfaces->gameEventManager->addListener(&listener[3], xorstr_("weapon_fire"));
		listener_registered = true;
	} else if ((!config->ragebot[weapon_index].resolver || force_remove) && listener_registered) {
		interfaces->gameEventManager->removeListener(listener.data());
		interfaces->gameEventManager->removeListener(&listener[1]);
		interfaces->gameEventManager->removeListener(&listener[2]);
		interfaces->gameEventManager->removeListener(&listener[3]);
		listener_registered = false;
	}
}