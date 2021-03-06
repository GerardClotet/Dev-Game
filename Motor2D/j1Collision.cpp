#include "j1App.h"
#include "j1Input.h"
#include "j1Render.h"
#include "p2Log.h"
#include "j1Collision.h"
#include "j1Scene.h"
#include "j1EntityFactory.h"
#include "j1Audio.h"
j1Collision::j1Collision()
{
	name.assign("collision");
}

// Destructor
j1Collision::~j1Collision()
{}

bool j1Collision::Awake(pugi::xml_node& config)
{
	max_colliders = 200;
	colliders = new Collider * [max_colliders];
	for (uint i = 0; i < max_colliders; ++i)
		colliders[i] = nullptr;
	return true;
}
bool j1Collision::Start()
{
	return true;
}

bool j1Collision::PreUpdate()
{
	// Remove colliders
	for (uint i = 0; i < max_colliders; ++i)
	{
		if (colliders[i] != nullptr && colliders[i]->to_delete == true)
		{
			delete colliders[i];
			colliders[i] = nullptr;
		}
	}

	if (player_collider)
	{
		for (uint i = 0; i < max_colliders; ++i)
		{
			// skip empty colliders
			if (colliders[i] != nullptr && colliders[i]->type == COLLIDER_TRIGGER)
			{
				if (player_collider->CheckCollision(colliders[i]->rect))
				{
					if (player_collider->callback && App->entityFactory->player->state != GOD)
					{
						player_collider->callback->OnCollision(player_collider, colliders[i]);
						
						if (!Triggercolliding)
						{
							App->entityFactory->ClearCoinVec();
							App->scene->TriggerColl();
							App->audio->PlayFx(App->scene->endLvlSFX, NO_REPEAT);
					
							Triggercolliding = true;
						}

					}
					if (colliders[i]->callback)
						colliders[i]->callback->OnCollision(colliders[i], player_collider);
				}
			}

			else if (colliders[i] != nullptr && colliders[i]->type == COLLIDER_COLLECTABLE)
			{
				if (player_collider->CheckCollision(colliders[i]->rect))
				{
 					if (player_collider->callback && App->entityFactory->player->state)
					{

						App->entityFactory->coins += 1;
						App->entityFactory->reward_counter += 1;
						if (App->entityFactory->reward_counter == 10)
						{
							App->entityFactory->IncreaseLifesBy(1);
							App->entityFactory->reward_counter = 0;

						}
						App->audio->PlayFx(App->scene->coinSFX);
						
						if (colliders[i]->callback)
							colliders[i]->callback->OnCollision(colliders[i], player_collider);

						
						colliders[i]->to_delete = true;
					}
				}
			}
		}
	}
	return true;
}

// Called before render is available
bool j1Collision::Update(float dt)
{
	DebugDraw();

	return true;
}
// Called before quitting
bool j1Collision::CleanUp()
{
	LOG("Freeing all colliders");

	for (uint i = 0; i < max_colliders; ++i)
	{
		if (colliders[i] != nullptr)
		{
			delete colliders[i];
			colliders[i] = nullptr;
		}
	}
	delete[] colliders;

	return true;
}



Collider* j1Collision::AddCollider(const SDL_Rect& rect, const ColliderType& type, j1Module* callback, const bool& player)
{
	Collider* ret = nullptr;

	for (uint i = 0; i < max_colliders; ++i)
	{
		if (colliders[i] == nullptr)
		{
			ret = colliders[i] = new Collider(rect, type, callback);
			if (player) player_collider = colliders[i];
			break;
		}
	}

	return ret;
}

void j1Collision::DebugDraw()
{
	if (App->input->GetKey(SDL_SCANCODE_F9) == KEY_DOWN)
	{
		debug = !debug;
		App->entityFactory->draw_path = !App->entityFactory->draw_path;
	}

	if (debug == false)
		return;

	Uint8 alpha = 80;
	for (uint i = 0; i < max_colliders; ++i)
	{
		if (colliders[i] == nullptr)
			continue;

		switch (colliders[i]->type)
		{
		case COLLIDER_NONE: // white
			App->render->DrawQuad(colliders[i]->rect, 255, 255, 255, alpha);
			break;
		case COLLIDER_PLATFORM: // blue
			App->render->DrawQuad(colliders[i]->rect, 0, 0, 255, alpha, true);
			break;
		case COLLIDER_FLOOR: // red
			App->render->DrawQuad(colliders[i]->rect, 255, 0, 0, alpha, true);
			break;
		case COLLIDER_PLAYER: // green
			App->render->DrawQuad(colliders[i]->rect, 0, 255, 0, alpha, true);
			break;
		case COLLIDER_TRIGGER: //yellow
			App->render->DrawQuad(colliders[i]->rect, 255, 255, 0, alpha, true);
			break;

		case COLLIDER_ENEMY:
			App->render->DrawQuad(colliders[i]->rect, 0, 255, 255, alpha, true);
			break;
		case COLLIDER_FLYING:
			App->render->DrawQuad(colliders[i]->rect, 130, 214, 237, alpha, true);
			break;
		case COLLIDER_COLLECTABLE:
			App->render->DrawQuad(colliders[i]->rect, 214, 50, 255, alpha, true);
			break;

		default:
			break;
		}
	}
}

void j1Collision::DeleteCollider(Collider* coll)
{

	for (uint i = 0; i < max_colliders; ++i)
	{
		if (colliders[i] == coll)
		{
			delete colliders[i];
			colliders[i] == nullptr;
			return;
		}
	}
}

bool Collider::CheckCollision(const SDL_Rect& r) const
{
	return  ((r.x < rect.x + rect.w) && (rect.x < r.x + r.w)
		&& (r.y < rect.y + rect.h) && (rect.y < r.y + r.h));
}


float j1Collision::DistanceToRightCollider(Collider* coll, Collider* &colltype) const
{
	float distance = 999;

	for (uint i = 0; i < max_colliders; i++)
	{
		if (colliders[i] != nullptr && colliders[i] != coll && colliders[i]->type != COLLIDER_TRIGGER && colliders[i]->type != COLLIDER_PLATFORM && colliders[i]->type != COLLIDER_COLLECTABLE) //check for valid collider
		{
			if (colliders[i]->rect.x > coll->rect.x) //check for right side of received collider
			{
				if (coll->rect.y < colliders[i]->rect.y + colliders[i]->rect.h && coll->rect.y + coll->rect.h > colliders[i]->rect.y) //possible collision
				{
					float new_distance = colliders[i]->rect.x - (coll->rect.x + coll->rect.w);
					if (new_distance < distance)
					{
						distance = new_distance;	
						if (distance == 0)
						{
							colltype = colliders[i];
						}
						
					}
				}
			}
		}
	}
	return distance;
}

float j1Collision::DistanceToLeftCollider(Collider* coll, Collider* &colltype) const
{
	float distance = -999;

	for (uint i = 0; i < max_colliders; i++)
	{
		if (colliders[i] != nullptr && colliders[i] != coll && colliders[i]->type != COLLIDER_TRIGGER && colliders[i]->type != COLLIDER_PLATFORM && colliders[i]->type != COLLIDER_COLLECTABLE)
		{
			if (colliders[i]->rect.x < coll->rect.x)
			{
				if (coll->rect.y < colliders[i]->rect.y + colliders[i]->rect.h && coll->rect.y + coll->rect.h > colliders[i]->rect.y)
				{
					float new_distance = (colliders[i]->rect.x + colliders[i]->rect.w) - coll->rect.x;
					if (new_distance > distance)
					{
						distance = new_distance;
						if (distance == 0)
						{
							colltype = colliders[i];
						}
					}
				}
			}
		}
	}
	return distance;
}

float j1Collision::DistanceToBottomCollider(Collider* coll, Collider*& colltype,bool ignore_platform ) const
{
	float distance = 999;

	for (uint i = 0; i < max_colliders; i++)
	{
		if (colliders[i] != nullptr && colliders[i] != coll && (ignore_platform ? colliders[i]->type == COLLIDER_FLOOR : (colliders[i]->type == COLLIDER_PLATFORM || colliders[i]->type == COLLIDER_FLOOR)))
		{
			if (colliders[i]->rect.y >= coll->rect.y + coll->rect.h)
			{
				if (coll->rect.x < colliders[i]->rect.x + colliders[i]->rect.w && coll->rect.x + coll->rect.w > colliders[i]->rect.x)
				{
					float new_distance = colliders[i]->rect.y - (coll->rect.y + coll->rect.h);
					if (new_distance < distance)
					{
						distance = new_distance;
						if (distance == 0)
						{
							colltype = colliders[i];
						}
					}
				}
			}
		}
	}
	return distance;
}

float j1Collision::DistanceToTopCollider(Collider* coll, Collider* &colltype) const
{
	float distance = -999;

	for (uint i = 0; i < max_colliders; i++)
	{
		if (colliders[i] != nullptr && colliders[i] != coll && colliders[i]->type != COLLIDER_TRIGGER && colliders[i]->type != COLLIDER_PLATFORM && colliders[i]->type != COLLIDER_COLLECTABLE)
		{
			if (colliders[i]->rect.y <= coll->rect.y)
			{
				if (coll->rect.x < colliders[i]->rect.x + colliders[i]->rect.w && coll->rect.x + coll->rect.w > colliders[i]->rect.x)
				{
					float new_distance = (colliders[i]->rect.y + colliders[i]->rect.h) - coll->rect.y;
					if (new_distance > distance)
					{
						distance = new_distance;
						if (distance == 0)
						{
							colltype = colliders[i];
						}
					}
				}
			}
		}
	}
	return distance;
}