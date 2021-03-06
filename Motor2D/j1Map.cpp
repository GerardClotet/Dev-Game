#include "p2Defs.h"
#include "p2Log.h"
#include "j1App.h"
#include "j1Render.h"
#include "j1Textures.h"
#include "j1Map.h"
#include "j1Scene.h"
#include "j1EntityFactory.h"
#include <math.h>

j1Map::j1Map() : j1Module(), map_loaded(false)
{
	name.assign("map");
}

// Destructor
j1Map::~j1Map()
{}

// Called before render is available
bool j1Map::Awake(pugi::xml_node& config)
{
	LOG("Loading Map Parser");
	bool ret = true;

	folder.create(config.child("folder").child_value());

	return ret;
}



bool j1Map::CreateWalkabilityMap(int& width, int& height, uchar** buffer) const
{
	bool ret = false;
	std::list<MapLayer*>::const_iterator item;
	item = data.layers.begin();

	for (; *item != NULL; ++item)
	{
		MapLayer* layer = *item;

		if (layer->name !="Navigation")
			continue;

		uchar * map = new uchar[layer->width * layer->height];
		memset(map, 1, layer->width * layer->height);

		for (int y = 0; y < data.height; ++y)
		{
			for (int x = 0; x < data.width; ++x)
			{
				int i = (y * layer->width) + x;

				int tile_id = layer->Get(x, y);
				TileSet* tileset = (tile_id > 0) ? GetTilesetFromTileId(tile_id) : NULL;

				if (tileset != NULL)
				{
					map[i] = (tile_id - tileset->firstgid) > 0 ? 0 : 1;

				}
			}
		}

		*buffer = map;
		width = data.width;
		height = data.height;
		ret = true;

		break;
	}

	return true;
}






void j1Map::Draw()
{
	if(map_loaded == false)
		return;
	if(data.backgroundimage!=nullptr)
		App->render->Blit(data.backgroundimage, 0, 0, &data.backgroundrectangle, 1.0f,1.0F);


	TileSet* tileset;
	SDL_Rect r;
	iPoint pos;
	iPoint cam;
	std::list<MapLayer*>::iterator item = data.layers.begin();
	int tilesdrawing = 0;
	for(; item != data.layers.end(); ++item)
	{
		(*item)->noDraw;
		MapLayer* layer = (*item);

		if (layer->noDraw != 0)
			continue;


		for(int y = 0; y < data.height; ++y)
		{
			for(int x = 0; x < data.width; ++x)
			{
				int tile_id = layer->Get(x, y);
				if(tile_id > 0)
				{
					tileset = GetTilesetFromTileId(tile_id);
					r = tileset->GetTileRect(tile_id);
					pos = MapToWorld(x, y);
					cam = WorldToMap(App->render->camera.x, App->render->camera.y);

					if (layer->parallaxSpeed != 1.0f) //for parallax layers
					{
						App->render->BlitMap(tileset->texture, pos.x, pos.y, &r, layer->parallaxSpeed, 1.0f/*speedY*/);
						tilesdrawing += 1;

					}
					else if (pos.x >= -App->render->camera.x - CAM_MARGIN && pos.x <= -App->render->camera.x  + App->render->camera.w + CAM_MARGIN) //doesn't blit tiles outisde camera rect --+PERFORMANCE+--
					{
						
						if (pos.y >= -App->render->camera.y - CAM_MARGIN && pos.y <= -App->render->camera.y  + App->render->camera.h + CAM_MARGIN)
						{
							App->render->BlitMap(tileset->texture, pos.x, pos.y, &r, layer->parallaxSpeed, 1.0f);
							tilesdrawing += 1;
						}
					}
				}
			}
		}
	}
	
}

int Properties::Get(const char* value, int default_value) const
{
	std::list<Property*>::const_iterator item;
	item = list.begin();

	while (item != list.end())
	{
		if ((*item)->name == value)
			return (*item)->value;
		++item;
	}

	return default_value;
}

float Properties::Getf(const char* value, float default_value) const
{
	std::list<Property*>::const_iterator item;
	item = list.begin();

	while (item != list.end())
	{
		if ((*item)->name == value)
			return (*item)->value;
		++item;
	}

	return default_value;
}

TileSet* j1Map::GetTilesetFromTileId(int id) const
{
	std::list<TileSet*>::const_iterator item = data.tilesets.begin();
	TileSet* set = *item;

	while (item != data.tilesets.end())
	{
		if (id < (*item)->firstgid)
		{
			set = *(--item);
			++item;
			break;
		}
		set = *item;
		++item;
	}

	return set;
}

bool j1Map::SwitchMaps(std::string newMap)
{
	App->entityFactory->DeleteAllEnemies();
	App->entityFactory->DeleteAllCollectables();
	CleanUp();
	
	App->scene->SwapMap();
	Load(newMap.data());
	App->scene->actualMap = newMap;
	return false;
}

iPoint j1Map::MapToWorld(int x, int y) const
{
	iPoint ret;

	if(data.type == MAPTYPE_ORTHOGONAL)
	{
		ret.x = x * data.tile_width;
		ret.y = y * data.tile_height;
	}
	else if(data.type == MAPTYPE_ISOMETRIC)
	{
		ret.x = (x - y) * (data.tile_width * 0.5f);
		ret.y = (x + y) * (data.tile_height * 0.5f);
	}
	else
	{
		LOG("Unknown map type");
		ret.x = x; ret.y = y;
	}

	return ret;
}

iPoint j1Map::WorldToMap(int x, int y) const
{
	iPoint ret(0,0);

	if(data.type == MAPTYPE_ORTHOGONAL)
	{
		ret.x = x / data.tile_width;
		ret.y = y / data.tile_height;
	}
	else if(data.type == MAPTYPE_ISOMETRIC)
	{
		
		float half_width = data.tile_width * 0.5f;
		float half_height = data.tile_height * 0.5f;
		ret.x = int( (x / half_width + y / half_height) / 2);
		ret.y = int( (y / half_height - (x / half_width)) / 2);
	}
	else
	{
		LOG("Unknown map type");
		ret.x = x; ret.y = y;
	}

	return ret;
}

SDL_Rect TileSet::GetTileRect(int id) const
{
	int relative_id = id - firstgid;
	SDL_Rect rect;
	rect.w = tile_width;
	rect.h = tile_height;
	rect.x = margin + ((rect.w + spacing) * (relative_id % num_tiles_width));
	rect.y = margin + ((rect.h + spacing) * (relative_id / num_tiles_width));
	return rect;
}

// Called before quitting
bool j1Map::CleanUp()
{
	LOG("Unloading map");

	// Remove all tilesets
	std::list<TileSet*>::iterator tiles_item;
	tiles_item = data.tilesets.begin();

	while (tiles_item != data.tilesets.end())
	{
		if ((*tiles_item) != nullptr && (*tiles_item)->texture != nullptr)
		{
			App->tex->UnLoad((*tiles_item)->texture); 
			(*tiles_item)->texture = nullptr;
		}
		delete* tiles_item;
		*tiles_item = nullptr;
		++tiles_item;
	}
	data.tilesets.clear(); 

	// Remove all layers
	std::list<MapLayer*>::iterator layer_item;
	layer_item = data.layers.begin();
	
	while (layer_item != data.layers.end()) //Aqui peta
	{
		data.layers.remove(*layer_item);
		
		delete* layer_item; //quan ++ layerItem algo pasa q	ue no lo pilla
		++layer_item;
		*layer_item = nullptr;

	}
	data.layers.clear();

	std::list<Collider*>::iterator collider_item;
	collider_item = data.colliders.begin();

	LOG("colliders size %i",data.colliders.size());

	while (collider_item != data.colliders.end())
	{
		(*collider_item)->to_delete = true;
		++collider_item;	
	}
	data.colliders.clear(); //version 16.1.5

	App->tex->UnLoad(data.backgroundimage);
	data.backgroundimage = nullptr;



	// Clean up the pugui tree
	map_file.reset();

	return true;
}

// Load new map
bool j1Map::Load(const char* file_name)
{
	bool ret = true;
	p2SString tmp("%s%s", folder.GetString(), file_name);
	pugi::xml_parse_result result = map_file.load_file(tmp.GetString());

	if(result == NULL)
	{
		LOG("Could not load map xml file %s. pugi error: %s", file_name, result.description());
		ret = false;
	}

	// Load general info ----------------------------------------------
	if(ret == true)
	{
		ret = LoadMap();
	}

	// Load all tilesets info ----------------------------------------------
	pugi::xml_node tileset;
	for (tileset = map_file.child("map").child("tileset"); tileset && ret; tileset = tileset.next_sibling("tileset"))
	{
		TileSet* set = new TileSet();

		if (ret == true)
		{
			ret = LoadTilesetDetails(tileset, set);
		}

		if (ret == true)
		{
			ret = LoadTilesetImage(tileset, set);
		}

		data.tilesets.push_back(set);
	}

	// Load layer info ----------------------------------------------
	pugi::xml_node layer;

	for (auto node : map_file.child("map").children("layer"))
	{
		MapLayer* lay = new MapLayer();
		ret = LoadLayer(node, lay);
		if (ret == true)
			data.layers.push_back(lay);
	}

	for(layer = map_file.child("map").child("layer"); layer && ret; layer = layer.next_sibling("layer"))
	{
		MapLayer* lay = new MapLayer();

		ret = LoadLayer(layer, lay);

		if(ret == true)
			data.layers.push_back(lay);
		
	}


	// load obj groups
	for (auto node : map_file.child("map").children("objectgroup")/*.child("animations").children("frame")*/)
	{
	
		
		std::string type = node.attribute("name").as_string();

		if (type == "collectables")
			LoadCollectableLayer(node);
		else if (type == "triggers")
			LoadUtilsLayer(node);

		else if (type == "colliders")
			LoadCollidersLayer(node);

		

		

		LOG("iterated objectgroup");
	}

	//Load Image Layer
	
	pugi::xml_node backgroundimage = map_file.child("map").child("imagelayer");
	data.backgroundimage = App->tex->Load(PATH(folder.GetString(), backgroundimage.child("image").attribute("source").as_string()));
	data.backgroundrectangle = { 0,0,backgroundimage.child("image").attribute("width").as_int(), backgroundimage.child("image").attribute("height").as_int() };

	if(ret == true)
	{
		LOG("Successfully parsed map XML file: %s", file_name);
		LOG("width: %d height: %d", data.width, data.height);
		LOG("tile_width: %d tile_height: %d", data.tile_width, data.tile_height);

		std::list<TileSet*>::iterator item = data.tilesets.begin();
		while (item != data.tilesets.end())
		{
			TileSet* s = (*item);
			LOG("Tileset ----");
			LOG("name: %s firstgid: %d", s->name.data(), s->firstgid);
			LOG("tile width: %d tile height: %d", s->tile_width, s->tile_height);
			LOG("spacing: %d margin: %d", s->spacing, s->margin);
			++item;
		}

		std::list<MapLayer*>::iterator item_layer = data.layers.begin();
		while (item_layer != data.layers.end())
		{
			MapLayer* l = (*item_layer);
			LOG("Layer ----");
			LOG("name: %s", l->name.data());
			LOG("tile width: %d tile height: %d", l->width, l->height);
			++item_layer;
		}
	}

	

	LOG("caMPOS %i %i", App->render->camera.x, App->render->camera.y);
	//LOG("playerpos %i %i",App->entityFactory->player->position.x, App->entityFactory->player->position.y);
	map_loaded = ret;
	return ret;
}

// Load map general properties
bool j1Map::LoadMap()
{
	bool ret = true;
	pugi::xml_node map = map_file.child("map");

	if(map == NULL)
	{
		LOG("Error parsing map xml file: Cannot find 'map' tag.");
		ret = false;
	}
	else
	{
		data.width = map.attribute("width").as_int();
		data.height = map.attribute("height").as_int();
		data.tile_width = map.attribute("tilewidth").as_int();
		data.tile_height = map.attribute("tileheight").as_int();
		p2SString bg_color(map.attribute("backgroundcolor").as_string());

		data.background_color.r = 0;
		data.background_color.g = 0;
		data.background_color.b = 0;
		data.background_color.a = 0;

		if(bg_color.Length() > 0)
		{
			p2SString red, green, blue;
			bg_color.SubString(1, 2, red);
			bg_color.SubString(3, 4, green);
			bg_color.SubString(5, 6, blue);

			int v = 0;

			sscanf_s(red.GetString(), "%x", &v);
			if(v >= 0 && v <= 255) data.background_color.r = v;

			sscanf_s(green.GetString(), "%x", &v);
			if(v >= 0 && v <= 255) data.background_color.g = v;

			sscanf_s(blue.GetString(), "%x", &v);
			if(v >= 0 && v <= 255) data.background_color.b = v;
		}

		std::string orientation(map.attribute("orientation").as_string());

		if(orientation == "orthogonal")
		{
			data.type = MAPTYPE_ORTHOGONAL;
		}
		else if(orientation == "isometric")
		{
			data.type = MAPTYPE_ISOMETRIC;
		}
		else if(orientation == "staggered")
		{
			data.type = MAPTYPE_STAGGERED;
		}
		else
		{
			data.type = MAPTYPE_UNKNOWN;
		}
	}

	return ret;
}

bool j1Map::LoadTilesetDetails(pugi::xml_node& tileset_node, TileSet* set)
{
	bool ret = true;
	set->name.assign(tileset_node.attribute("name").as_string());
	set->firstgid = tileset_node.attribute("firstgid").as_int();
	set->tile_width = tileset_node.attribute("tilewidth").as_int();
	set->tile_height = tileset_node.attribute("tileheight").as_int();
	set->margin = tileset_node.attribute("margin").as_int();
	set->spacing = tileset_node.attribute("spacing").as_int();
	pugi::xml_node offset = tileset_node.child("tileoffset");

	if(offset != NULL)
	{
		set->offset_x = offset.attribute("x").as_int();
		set->offset_y = offset.attribute("y").as_int();
	}
	else
	{
		set->offset_x = 0;
		set->offset_y = 0;
	}

	return ret;
}

bool j1Map::LoadTilesetImage(pugi::xml_node& tileset_node, TileSet* set)
{
	bool ret = true;
	pugi::xml_node image = tileset_node.child("image");

	if(image == NULL)
	{
		LOG("Error parsing tileset xml file: Cannot find 'image' tag.");
		ret = false;
	}
	else
	{
		LOG("%s", image.attribute("source").as_string());
		set->texture = App->tex->Load(PATH(folder.GetString(), image.attribute("source").as_string()));
		int w, h;
		SDL_QueryTexture(set->texture, NULL, NULL, &w, &h);
		set->tex_width = image.attribute("width").as_int();

		if(set->tex_width <= 0)
		{
			set->tex_width = w;
		}

		set->tex_height = image.attribute("height").as_int();

		if(set->tex_height <= 0)
		{
			set->tex_height = h;
		}

		set->num_tiles_width = set->tex_width / set->tile_width;
		set->num_tiles_height = set->tex_height / set->tile_height;
	}

	return ret;
}

bool j1Map::LoadLayer(pugi::xml_node& node, MapLayer* layer)
{
	bool ret = true;

	layer->name = node.attribute("name").as_string();
	layer->width = node.attribute("width").as_int();
	layer->height = node.attribute("height").as_int();
	layer->parallaxSpeed = node.child("properties").child("property").attribute("value").as_float(0.0f);


	LoadProperties(node, layer->properties);
	
	if (layer->properties.Get("walkable") == 1);
		

	 layer->noDraw= layer->properties.Get("rNodraw");
	// layer->parallaxSpeed = layer->properties.Getf("parallaxSpeed");
	 pugi::xml_node layer_data = node.child("data");


	if(layer_data == NULL)
	{
		LOG("Error parsing map xml file: Cannot find 'layer/data' tag.");
		ret = false;
		RELEASE(layer);
	}
	else
	{
		layer->data = new uint[layer->width*layer->height];
		memset(layer->data, 0, layer->width*layer->height);

		int i = 0;
		for(pugi::xml_node tile = layer_data.child("tile"); tile; tile = tile.next_sibling("tile"))
		{
			layer->data[i++] = tile.attribute("gid").as_int(0);
		}
	}

	return ret;
}


// Load a group of properties from a node and fill a list with it
bool j1Map::LoadProperties(pugi::xml_node& node, Properties& properties)
{
	bool ret = false;

	pugi::xml_node data = node.child("properties");

	if(data != NULL)
	{
		pugi::xml_node prop;

		for(prop = data.child("property"); prop; prop = prop.next_sibling("property"))
		{
			Properties::Property* p = new Properties::Property();

			p->name = prop.attribute("name").as_string();
			p->value = prop.attribute("value").as_int();
			properties.list.push_back(p);
		}
	}

	return ret;
}



void j1Map::LoadCollidersLayer(pugi::xml_node& node)
{



	pugi::xml_node_iterator it = node/*.child("map").child("objectgroup")*/.begin();
	while (it != node.end())
	{


		SDL_Rect rect = { (*it).attribute("x").as_int(),(*it).attribute("y").as_int(), (*it).attribute("width").as_int(), (*it).attribute("height").as_int() };

		if (collFloor == (*it).attribute("type").as_string())
			data.colliders.push_back(App->collision->AddCollider(rect, COLLIDER_FLOOR));

		else if (collPlatform == (*it).attribute("type").as_string())
			data.colliders.push_back(App->collision->AddCollider(rect, COLLIDER_PLATFORM));

		++it;
	}
	
}



void j1Map::LoadUtilsLayer(pugi::xml_node& node)
{


	pugi::xml_node_iterator it = node.begin();
	while (it != node.end())
	{


		SDL_Rect rect = { (*it).attribute("x").as_int(),(*it).attribute("y").as_int(), (*it).attribute("width").as_int(), (*it).attribute("height").as_int() };

		if (strcmp("start", (*it).attribute("type").as_string()) == 0)
		{
			App->entityFactory->CreateEntity({ (*it).attribute("x").as_int(), (*it).attribute("y").as_int() }, E_TYPE::PLAYER);
			if (App->entityFactory->previous_play)
			{
				App->entityFactory->player->SetPos(App->entityFactory->temp_pos);
			}
	
		}

		else if (strcmp("end", (*it).attribute("type").as_string())==0)
			data.colliders.push_back(App->collision->AddCollider(rect, COLLIDER_TRIGGER));


		
		else if (strcmp("test_enemy", (*it).attribute("type").as_string()) == 0)
			App->entityFactory->CreateEntity({ (*it).attribute("x").as_int(), (*it).attribute("y").as_int() }, E_TYPE::WALK_E);


		

		else if (strcmp("FlyingEnemy" , (*it).attribute("type").as_string())==0)
			App->entityFactory->CreateEntity({ (*it).attribute("x").as_int(), (*it).attribute("y").as_int() }, E_TYPE::FLY_E);

		++it;
	}

	if (App->entityFactory->player != nullptr )
	{
		App->entityFactory->CheckifCoinsWereTaken();
		
	}

}

void j1Map::LoadCollectableLayer(pugi::xml_node& node)
{

	j1Entity* temp_ent= nullptr;
	int id = 0;
	pugi::xml_node_iterator it = node.begin();
	while (it != node.end())
	{

		if (strcmp("coin", (*it).attribute("type").as_string()) == 0)
		{
			temp_ent = App->entityFactory->CreateEntity({ (*it).attribute("x").as_int(), (*it).attribute("y").as_int() }, E_TYPE::COLLECTABLE);

			temp_ent->Setid(id);
			id++;
		}

		++it;
	}
	

}


