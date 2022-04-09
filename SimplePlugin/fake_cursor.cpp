#include "../plugin_sdk/plugin_sdk.hpp"
#include <cmath>
#include <cassert>
#include "fake_cursor.h"

namespace fake_cursor
{

	TreeTab* main_tab = nullptr;

	namespace menu_gamerfakecursor
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* first_aa_speed = nullptr;
		TreeEntry* to_target = nullptr;
		TreeEntry* from_target = nullptr;
		TreeEntry* inbetween_aa_delay = nullptr;
		TreeEntry* curSize = nullptr;
	}

	namespace menu_spells
	{
		TreeEntry* spell_delay = nullptr;
	}
	namespace menu_evade
	{
		TreeEntry* evade_delay = nullptr;
	}

	void on_update();
	void on_draw();
	void on_before_attack(game_object_script target, bool* process);
	void on_evade();

	loaded_texture* normal;
	loaded_texture* tep;
	loaded_texture* he;
	loaded_texture* ha;

	bool is_casting_spell = false;
	bool is_attacking = false;
	bool is_first_aa_done = false;
	bool isafterattack = false;

	float beforeattack;
	float afterattack;
	float lastAttack = 0;
	float lastEvade = 0;
	float lastSpellCast = 0;
	float get_last_aa_time = 0;
	float get_attack_delay = 0;
	float get_attack_cast_delay = 0;

	std::int32_t* sv = 0;
	vector curpos;
	void on_after_attack(game_object_script target);

	void on_cast_spell(spellslot spell_slot, game_object_script target, vector& pos, vector& pos2, bool is_charge, bool* process);

	void load()
	{
		curpos = vector(game_input->get_cursor_pos().x, game_input->get_cursor_pos().y);

		normal = draw_manager->load_texture_from_file(
			L"normal.png");
		tep = draw_manager->load_texture_from_file(
			L"target_enemy_precise.png");
		he = draw_manager->load_texture_from_file(
			L"hover_enemy_precise.png");
		ha = draw_manager->load_texture_from_file(
			L"hover_ally_precise.png");


		srand(time(NULL));
		main_tab = menu->create_tab("gamerfakercursor", "GamerFakeCursor");
		{
			menu_gamerfakecursor::curSize = main_tab->add_slider(".psize", "Cursor Size", 42, 10, 100);
			auto combo = main_tab->add_tab(myhero->get_model() + ".humanizer", "Humanizer");
			{
				menu_gamerfakecursor::inbetween_aa_delay = combo->add_slider(".delayv", "Inbetween AA Delay Position", 0, -100, 100);
				menu_gamerfakecursor::first_aa_speed = combo->add_slider(".delayaav", "First AA Speed", 0, -20, 20);
				menu_gamerfakecursor::to_target = combo->add_slider(".totar", "To Target", 0, -20, 20);
				menu_gamerfakecursor::from_target = combo->add_slider(".frtar", "From Target", 0, -20, 20);
				
			}
			auto menu_spells = combo->add_tab(".spellhumanizer", "Spells Delay");
			{
				menu_spells::spell_delay = menu_spells->add_slider(".delayspell", "Spell Cast Delay", 0, -20, 20);
			}
			auto menu_evade = combo->add_tab(".evadehumanizer", "Evade Delay");
			{
				
				menu_evade::evade_delay = menu_evade->add_slider(".delayevade", "Evade Cast Delay", 0, -20, 20);
			}
		}
		
		event_handler<events::on_update>::add_callback(on_update);
		event_handler<events::on_before_attack_orbwalker>::add_callback(on_before_attack);
		event_handler<events::on_cast_spell>::add_callback(on_cast_spell);
		event_handler<events::on_after_attack_orbwalker>::add_callback(on_after_attack);
		event_handler<events::on_draw>::add_callback(on_draw);

	}

	void unload()
	{

		event_handler<events::on_update>::remove_handler(on_update);
		event_handler<events::on_draw>::remove_handler(on_draw);
		event_handler<events::on_cast_spell>::remove_handler(on_cast_spell);
		event_handler<events::on_before_attack_orbwalker>::remove_handler(on_before_attack);

		event_handler<events::on_after_attack_orbwalker>::remove_handler(on_after_attack);
	}


	float lint1(float a, float b, float f) {
		auto f2 = std::clamp(f, 0.0f, 1.0f);
		return (a * (1.0f - f2)) + (b * f2);
	}

	float smoothstep(float edge0, float edge1, float x) {
		// Scale, bias and saturate x to 0..1 range
		x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		// Evaluate polynomial
		return x * x * (3 - 2 * x);
	}

	game_object_script drtar;
	clock_t deltaTime;
	clock_t oldTime;
	float time_established;
	float total_move_time = 152;
	float lp1;
	float lp2;
	float psize = 42;
	vector oi;
	vector target_position;
	vector current_draw_cursor_position;
	bool is_done_from_start = false;
	bool should_interupt = false;
	
	bool should_move_cursor = false;
	bool is_evading = false;

	bool IsHoveringEnemy()
	{
		for (auto enemy : plugin_sdk->get_entity_list()->get_enemy_heroes())
		{
			if (!enemy->is_valid() || enemy->is_dead()) continue;
			vector ep;
			renderer->world_to_screen(enemy->get_position(), ep);
			auto cpt = vector(game_input->get_game_cursor_pos().x, game_input->get_game_cursor_pos().y).distance(ep);

			if (cpt < 100.0f)
			{
				return true;
			}
		}
		for (auto enemy : plugin_sdk->get_entity_list()->get_enemy_minions())
		{
			if (!enemy->is_valid() || enemy->is_dead()) continue;
			vector ep = target_position;
			renderer->world_to_screen(enemy->get_position(), ep);
			auto cpt = vector(game_input->get_game_cursor_pos().x, game_input->get_game_cursor_pos().y).distance(ep);

			if (cpt < 100.0f)
			{
				return true;
			}
		}
		for (auto enemy : plugin_sdk->get_entity_list()->get_all_turrets())
		{
			if (!enemy->is_valid() || enemy->is_dead() || enemy->get_team() == myhero->get_team()) continue;
			vector ep;
			renderer->world_to_screen(enemy->get_position(), ep);
			auto cpt = vector(game_input->get_game_cursor_pos().x, game_input->get_game_cursor_pos().y).distance(ep);

			if (cpt < 100.0f)
			{
				return true;
			}
		}
		return false;
	}

	void on_draw()
	{
		deltaTime = clock() - oldTime;
		oldTime = clock();
		if (evade->is_evading())
		{
			if (evade->evading_pos().x != 0)
				on_evade();
		}
		if (!is_attacking)
		{
			curpos = game_input->get_cursor_pos();
			bool isHE = IsHoveringEnemy();
			
			if (isHE)
			{

				draw_manager->add_image(he->texture, curpos, vector(psize, psize));
			}
			else
			{
				for (auto enemy : plugin_sdk->get_entity_list()->get_ally_heroes())
				{
					if (!enemy->is_valid() || enemy->is_dead() || enemy->is_me()) continue;
					
					vector ep = target_position;
					renderer->world_to_screen(enemy->get_position(), ep);
					auto cpt = vector(game_input->get_game_cursor_pos().x, game_input->get_game_cursor_pos().y).distance(ep);

					if (cpt < 100.0f)
					{
						draw_manager->add_image(ha->texture, curpos, vector(psize, psize));
						is_done_from_start = false;
						return;
					}
				}
				draw_manager->add_image(normal->texture, curpos, vector(psize, psize));
			}
			is_done_from_start = false;
			return;
		}

		if (!should_move_cursor)
		{
			is_attacking = false;
			time_established = 0;
			is_done_from_start = true;
			should_interupt = false;
			is_casting_spell = false;
			is_evading = false;
			return;
		}
		if (orbwalker->none_mode())
		{
			auto gt = plugin_sdk->get_game_time()->get_time();
			if (lastAttack + 1 < gt && lastEvade + 1 < gt && lastSpellCast + 1 < gt)
			{
				is_attacking = false;
				time_established = 0;
				is_done_from_start = true;
				should_interupt = false;
				should_move_cursor = false;
				is_casting_spell = false;
				is_evading = false;
				return;
			}
		}
		auto gt = plugin_sdk->get_game_time()->get_time();
		if (lastAttack + 1 < gt && lastEvade + 1 < gt && lastSpellCast + 1 < gt)
		{

			is_attacking = false;
			time_established = 0;
			is_done_from_start = true;
			should_interupt = false;
			should_move_cursor = false;
			is_casting_spell = false;
			is_evading = false;
			return;
		}
		if (should_move_cursor)
		{

			if (should_interupt)
			{
				if (!is_done_from_start)
				{

					if (time_established < total_move_time)
					{
						vector mir;
						vector mir2 = target_position;

						mir = current_draw_cursor_position;
						total_move_time += menu_gamerfakecursor::to_target->get_int();
						auto smooth_time = smoothstep(0.0f, 1.0f, time_established / (total_move_time - 60));


						if (myhero->get_attack_delay() <= 0.56 && lastAttack + 0.07 > plugin_sdk->get_game_time()->get_time())
						{
							oi = vector(lint1(mir.x, mir2.x + lp1, smoothstep(0.0f, 1.0f, time_established / (total_move_time))), lint1(mir.y, mir2.y + lp2, smoothstep(0.0f, 1.0f, time_established / (total_move_time))));
						}
						else
						{
							oi = vector(lint1(mir.x, mir2.x + lp1, smooth_time), lint1(mir.y, mir2.y + lp2, smoothstep(0.0f, 1.0f, time_established / (total_move_time - 60))));
						}

						/*if (smooth_time > 0.2)
						{

							draw_manager->add_image(tep->texture, oi, vector(50, 50));
						}
						else
						{
							draw_manager->add_image(textur->texture, oi, vector(50, 50));
						}*/
						draw_manager->add_image(tep->texture, oi, vector(psize, psize));

						time_established += deltaTime;
						return;
					}
					else
					{
						time_established = 0;
						is_done_from_start = true;
						return;
					}
				}
				if (time_established < total_move_time)
				{
					vector mir = target_position;
					vector mir2;
					mir2 = current_draw_cursor_position;
					total_move_time += menu_gamerfakecursor::from_target->get_int();
					if (myhero->get_attack_delay() <= 0.56 && lastAttack + 0.07 > plugin_sdk->get_game_time()->get_time())
					{
						oi = vector(lint1(mir.x + lp1, mir2.x, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 30))), lint1(mir.y + lp1, mir2.y, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 30))));
					}
					else
					{
						oi = vector(lint1(mir.x + lp1, mir2.x, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 50))), lint1(mir.y + lp1, mir2.y, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 50))));
					}


					draw_manager->add_image(normal->texture, oi, vector(psize, psize));

					time_established += deltaTime;
					return;
				}
				else
				{
					is_attacking = false;
					time_established = 0;
					is_done_from_start = true;
					should_interupt = false;
					should_move_cursor = false;
					is_casting_spell = false;
					is_evading = false;
					return;
				}

			}



			if (!is_done_from_start)
			{

				if (time_established <= total_move_time)
				{
					vector mir;
					vector mir2 = target_position;

					mir = game_input->get_cursor_pos();
					auto smooth_time = smoothstep(0.0f, 1.0f, time_established / (total_move_time - 60));
					total_move_time += menu_gamerfakecursor::to_target->get_int();

					if (myhero->get_attack_delay() <= 0.56 && lastAttack + 0.07 > plugin_sdk->get_game_time()->get_time())
					{
						oi = vector(lint1(mir.x, mir2.x + lp1, smoothstep(0.0f, 1.0f, time_established / (total_move_time))), lint1(mir.y, mir2.y + lp2, smoothstep(0.0f, 1.0f, time_established / (total_move_time))));
					}
					else
					{
						oi = vector(lint1(mir.x, mir2.x + lp1, smooth_time), lint1(mir.y, mir2.y + lp2, smoothstep(0.0f, 1.0f, time_established / (total_move_time - 60))));
					}

					current_draw_cursor_position = oi;

					draw_manager->add_image(tep->texture, oi, vector(psize, psize));

					time_established += deltaTime;
					return;
				}
				else
				{
					time_established = 0;
					is_done_from_start = true;
				}
			}
			if (time_established <= total_move_time)
			{
				vector mir = target_position;
				vector mir2;
				mir2 = game_input->get_cursor_pos();
				total_move_time += menu_gamerfakecursor::from_target->get_int();
				if (myhero->get_attack_delay() <= 0.56 && lastAttack + 0.07 > plugin_sdk->get_game_time()->get_time())
				{
					oi = vector(lint1(mir.x + lp1, mir2.x, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 30))), lint1(mir.y + lp1, mir2.y, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 30))));
				}
				else
				{
					oi = vector(lint1(mir.x + lp1, mir2.x, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 50))), lint1(mir.y + lp1, mir2.y, smoothstep(0.0f, 1.0f, time_established / (total_move_time + 50))));
				}

				current_draw_cursor_position = oi;
				draw_manager->add_image(normal->texture, oi, vector(psize, psize));

				time_established += deltaTime;
				return;
			}
			else
			{
				is_attacking = false;
				time_established = 0;
				is_done_from_start = true;
				should_move_cursor = false;
				should_interupt = false;
				is_casting_spell = false;
				is_evading = false;
			}
		}
		else
		{
			curpos = game_input->get_cursor_pos();
			bool isHE = IsHoveringEnemy();

			if (isHE)
			{

				draw_manager->add_image(he->texture, curpos, vector(psize, psize));
			}
			else
			{
				for (auto enemy : plugin_sdk->get_entity_list()->get_ally_heroes())
				{
					if (!enemy->is_valid() || enemy->is_dead() || enemy->is_me()) continue;

					vector ep = target_position;
					renderer->world_to_screen(enemy->get_position(), ep);
					auto cpt = vector(game_input->get_game_cursor_pos().x, game_input->get_game_cursor_pos().y).distance(ep);

					if (cpt < 100.0f)
					{
						draw_manager->add_image(ha->texture, curpos, vector(psize, psize));
						is_done_from_start = false;
						return;
					}
				}
				draw_manager->add_image(normal->texture, curpos, vector(psize, psize));
			}
		}
	}
	float afterba;

	void on_evade()
	{
		if (!myhero->can_move()) return;
		if (is_evading) return;
		if (lastEvade + 1.3 > plugin_sdk->get_game_time()->get_time()) return;
		total_move_time = 195;
		total_move_time += menu_evade::evade_delay->get_int();
		auto exevpo = (evade->evading_pos() - myhero->get_position()).normalized();
		auto msp2 = myhero->get_position() + (exevpo * 301);
		renderer->world_to_screen(msp2, target_position);
		if (is_attacking)
		{
			renderer->world_to_screen(msp2, target_position);
			is_done_from_start = false;
			time_established = 0;
			should_interupt = true;
		}
		lp1 = rand() % 20 + (-20);
		lp2 = rand() % 20 + (-20);
		beforeattack = plugin_sdk->get_game_time()->get_time();
		lastEvade = plugin_sdk->get_game_time()->get_time();
		should_move_cursor = true;
		is_attacking = true;
		is_evading = true;
	}
	float lastAttackFirst;
	void between_attacks()
	{
		if (is_casting_spell) return;
		if (orbwalker->none_mode()) return;

		if (evade->is_evading() || is_evading)return;
		if (!isafterattack) return;
		auto aa_reset = false;
		if (orbwalker->get_last_aa_time() == 0 && is_done_from_start)
		{
			aa_reset = true;
		}
		auto full_aa_time = orbwalker->get_last_aa_time() + get_attack_cast_delay + get_attack_delay;
		auto bty = get_attack_cast_delay / 1.7;
		auto auto_starting_point = get_attack_cast_delay + ((get_attack_delay / 2) + bty);
		auto_starting_point += menu_gamerfakecursor::inbetween_aa_delay->get_int() / 1000;

		if (get_attack_delay + get_attack_cast_delay < 0.305)
		{
			auto_starting_point -= get_attack_cast_delay / 3;
		}
		else if (get_attack_delay + get_attack_cast_delay < 0.350)
		{
			auto_starting_point -= get_attack_cast_delay + (get_attack_cast_delay / 3);
		}
		else if (get_attack_delay + get_attack_cast_delay < 0.485)
		{
			auto_starting_point -= get_attack_cast_delay + (get_attack_cast_delay / 3);
		}

		auto rng = rand() % 30 + (-15);
		total_move_time = 165 + rng;
		if (get_attack_delay <= 0.30)
		{
			total_move_time = 148 + rng;
		}
		else if (get_attack_delay <= 0.71)
		{
			total_move_time = 140 + rng;
		}
		


		if ((total_move_time)+5 > auto_starting_point)
		{
			total_move_time = 140 + rng;
			//console->print("fast");
		}

		if (get_attack_delay + get_attack_cast_delay < 0.305)
		{
			total_move_time = 105 + rng;
		}
		else if (get_attack_delay + get_attack_cast_delay < 0.350)
		{
			total_move_time = 110 + rng;
			if (get_attack_cast_delay > 0.057117)
			{
				auto_starting_point -= 0.1;
			}
		}
		else if (get_attack_delay + get_attack_cast_delay < 0.485)
		{
			total_move_time = 110 + rng;
			if (get_attack_cast_delay > 0.057117)
			{
				auto_starting_point -= 0.1;
			}
		}

		if (full_aa_time < gametime->get_time() + auto_starting_point + ((total_move_time / 1000) * 2)  && lastAttackFirst + 0.1 > gametime->get_time())
		{
			auto_starting_point = (get_attack_delay / 3) - (get_attack_delay / 2);
			total_move_time /= 1.3;
		}
		
		
		
		if (orbwalker->get_last_aa_time() + auto_starting_point < plugin_sdk->get_game_time()->get_time() && orbwalker->get_last_aa_time() > 0  || aa_reset)
		{
			total_move_time += menu_gamerfakecursor::first_aa_speed->get_int();
			//console->print("delay: %f", myhero->get_attack_delay());
			//console->print("delay+ex: %f", myhero->get_attack_delay() + myhero->get_attack_cast_delay());

			

			renderer->world_to_screen(orbwalker->get_target()->get_position(), target_position);

			lp1 = rand() % 80 + (-50);
			lp2 = rand() % 80 + (-50);
			if (get_attack_delay + get_attack_cast_delay < 0.430)
			{
				lp1 = rand() % 60 + (-20);
				lp2 = rand() % 60 + (-20);
			}

			beforeattack = plugin_sdk->get_game_time()->get_time();
			lastAttack = plugin_sdk->get_game_time()->get_time();
			should_move_cursor = true;
			is_attacking = true;
			isafterattack = false;
		}

	}

	

	void on_before_attack(game_object_script target, bool* process)
	{
		
		if (orbwalker->none_mode()) return;

		if (is_casting_spell) return;

		if (is_first_aa_done)
		{
			return;
		}
		if (evade->is_evading() || is_evading)return;
		if (!orbwalker->can_attack())return;
		if (!myhero->is_in_auto_attack_range(target) || orbwalker->get_target() == nullptr) return;
		auto rng = rand() % 30 + (-15);
		total_move_time = 155 + rng;
		auto get_adelay = myhero->get_attack_delay();
		auto get_acdelay = myhero->get_attack_cast_delay();
		if (get_adelay <= 0.6)
		{
			total_move_time = 134 + rng;
			if (get_adelay <= 0.32)
			{
				total_move_time = 97 + rng;
			}
			else if (get_adelay <= 0.42)
			{
				total_move_time = 124 + rng;
			}

			if (get_adelay + get_acdelay < 0.453 && get_adelay + get_acdelay < 0.413)
			{
				total_move_time = 95;
			}

			
			if (lastAttack + get_acdelay - 0.02 < plugin_sdk->get_game_time()->get_time() && is_attacking)
			{
				renderer->world_to_screen(target->get_position(), target_position);
				should_interupt = true;
				lp1 = rand() % 100 + (-70);
				lp2 = rand() % 100 + (-70);
				beforeattack = plugin_sdk->get_game_time()->get_time();
				drtar = target;
				lastAttack = plugin_sdk->get_game_time()->get_time();
				lastAttackFirst = plugin_sdk->get_game_time()->get_time();
				should_move_cursor = true;
				is_attacking = true;
				is_first_aa_done = true;
				return;
			}
		}
		renderer->world_to_screen(target->get_position(), target_position);
		if (beforeattack + 0.23 < plugin_sdk->get_game_time()->get_time() && is_attacking && lastAttack + get_acdelay < plugin_sdk->get_game_time()->get_time())
		{
			renderer->world_to_screen(target->get_position(), target_position);
			//itr = true;
		}
		if (is_attacking)
		{
			return;
		}
		lp1 = rand() % 80 + (-50);
		lp2 = rand() % 80 + (-50);
		beforeattack = plugin_sdk->get_game_time()->get_time();
		drtar = target;
		lastAttack = plugin_sdk->get_game_time()->get_time();
		lastAttackFirst = plugin_sdk->get_game_time()->get_time();
		should_move_cursor = true;
		is_attacking = true;
		is_first_aa_done = true;
	}

	void on_after_attack(game_object_script target)
	{

		isafterattack = true;
		afterba = plugin_sdk->get_game_time()->get_time();
	}

	spellslot lastSpell = spellslot::invalid;
	void on_cast_spell(spellslot spell_slot, game_object_script target, vector& pos, vector& pos2, bool is_charge,
		bool* process)
	{
		if (current_draw_cursor_position.distance(pos) < 150) return;
		if (myhero->get_champion() == champion_id::Vayne)
		{
			if (spell_slot == spellslot::q) return;
		}
		if (!myhero->can_cast()) return;
		if (is_evading) return;
		if (is_charge)return;
		if (pos.x == 0) return;
		if (lastSpell == spell_slot)
		{
			if (lastSpellCast + 2.2 > plugin_sdk->get_game_time()->get_time()) return;
		}
		

		total_move_time = 190;
		renderer->world_to_screen(pos, target_position);
		if (beforeattack + 0.23 < plugin_sdk->get_game_time()->get_time() && is_attacking)
		{
			renderer->world_to_screen(pos, target_position);
			should_interupt = true;
		}
		if (is_attacking)
		{
			return;
		}
		is_casting_spell = true;
		lp1 = rand() % 100 + (-70);
		lp2 = rand() % 100 + (-70);
		total_move_time += menu_spells::spell_delay->get_int();
		beforeattack = plugin_sdk->get_game_time()->get_time();
		drtar = target;
		should_move_cursor = true;

		is_attacking = true;
	}


	

	void on_update()
	{
		psize = menu_gamerfakecursor::curSize->get_int();
		curpos = vector(game_input->get_cursor_pos().x, game_input->get_cursor_pos().y);
		
		if (myhero->is_dead() )
		{
			return;
		}
		if (is_first_aa_done)
		{
			if (orbwalker->get_target()->is_valid())
			{
				get_attack_delay = myhero->get_attack_delay();
				get_attack_cast_delay = myhero->get_attack_cast_delay();
				between_attacks();
			}
			else
			{
				
				is_first_aa_done = false;
			}
		}

		return;

		
	}

}


