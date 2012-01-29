#include "TerrainTile.h"
#include "../App.h"
#include "TerrainManager.h"
#include "../gui/GUIManager.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace std;

//
const irr::f32 TerrainTile::vegetationRange = 60; // old value=60


TerrainTile::TerrainTile(ISceneManager* smgr, ISceneNode* parent, vector3df pos, stringc name)
{
    this->smgr=smgr;

    scale = TerrainManager::getInstance()->getScale();

    stringc tilename = TerrainManager::getInstance()->getTileMeshName();
	#ifdef APP_DEBUG
	printf("DEBUG : TERRAIN MANAGER : Here is the tile name: %s\n",tilename.c_str());
    #endif

	if (tilename=="")
		tilename="../media/land.obj";

	static IMesh* baseMesh = smgr->getMesh(tilename.c_str());

	SMesh* newMesh = NULL;

	newMesh = smgr->getMeshManipulator()->createMeshCopy(baseMesh);

	newMesh->setHardwareMappingHint(EHM_STATIC);
	//node=smgr->addOctreeSceneNode(newMesh,parent,100,512,true);  // Now work. Will be able to define octrees!
	//newMesh->setMaterialFlag(EMF_WIREFRAME ,true);
    node=smgr->addMeshSceneNode(newMesh,parent,100);
	nodescale = node->getBoundingBox().getExtent().X;
	TerrainManager::getInstance()->setTileMeshSize(nodescale);

    ocean=smgr->addMeshSceneNode(newMesh,node,0);
	
	//Testing if I can use Irrlicht generated mesh for base
	//ocean=smgr->addMeshSceneNode(smgr->addHillPlaneMesh("water",dimension2df(scale/nodescale,scale/nodescale),dimension2du(1,1)),node,0);

    node->setScale(vector3df(scale/nodescale,scale/nodescale,scale/nodescale));

    node->setPosition(pos*scale);

    selector = smgr->createTriangleSelector(newMesh,node);
    node->setTriangleSelector(selector);

    //node->setMaterialTexture(0,smgr->getVideoDriver()->getTexture("../0.jpg"));

    int tileX = round32(pos.X);
    int tileZ = round32(pos.Z);

    //cout << name.c_str() << endl;

    node->setName(name);

	stringc texture1 = TerrainManager::getInstance()->getTerrainTexture(1);
	stringc texture2 = TerrainManager::getInstance()->getTerrainTexture(2);
	stringc texture3 = TerrainManager::getInstance()->getTerrainTexture(3);
	stringc texture4 = TerrainManager::getInstance()->getTerrainTexture(4);

	static ITexture* layer0 = smgr->getVideoDriver()->getTexture(texture1.c_str());
	static ITexture* layer1 = smgr->getVideoDriver()->getTexture(texture2.c_str());
	static ITexture* layer2 = smgr->getVideoDriver()->getTexture(texture3.c_str());
    static ITexture* layer3 = smgr->getVideoDriver()->getTexture(texture4.c_str());

    //Create a Custom GLSL Material (Terrain Splatting)
    static s32 materialTerrain=smgr->getVideoDriver()->getGPUProgrammingServices()->addHighLevelShaderMaterialFromFiles(
        "../media/shaders/splat.vert", "vertexMain", video::EVST_VS_1_1,
        "../media/shaders/splat.frag", "pixelMain", video::EPST_PS_1_4,
        ShaderCallBack::getInstance(), video::EMT_SOLID); 

    //Assign Textures
    node->setMaterialTexture(0,layer0);
    node->setMaterialTexture(1,layer1);
    node->setMaterialTexture(2,layer2);
    node->setMaterialTexture(3,layer3);

    //Assign GLSL Shader
	node->getMaterial(0).setFlag(EMF_LIGHTING,false);
	node->getMaterial(0).setFlag(EMF_FOG_ENABLE,true);

    node->setMaterialType((E_MATERIAL_TYPE)materialTerrain); 

    //Create a Custom GLSL Material (Water shader)
   static s32 materialOcean=smgr->getVideoDriver()->getGPUProgrammingServices()->addHighLevelShaderMaterialFromFiles(
        "../media/shaders/ocean.vert", "vertexMain", video::EVST_VS_1_1,
        "../media/shaders/ocean.frag", "pixelMain", video::EPST_PS_1_4,
        ShaderCallBack::getInstance(), video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    static ITexture* oceanLayer0 = smgr->getVideoDriver()->getTexture("../media/waveNM.png");
    static ITexture* oceanLayer1 = smgr->getVideoDriver()->getTexture("../media/sky.jpg");

    //Assign GLSL Shader
    ocean->setMaterialType((E_MATERIAL_TYPE)materialOcean); 
	
    //Assign Textures
    ocean->setMaterialTexture(0,oceanLayer0);
    ocean->setMaterialTexture(1,oceanLayer1); 

    ocean->setMaterialFlag(EMF_FOG_ENABLE,true);
	srand ( App::getInstance()->getDevice()->getTimer()->getRealTime());

}

TerrainTile::~TerrainTile()
{
    for(int i=0;i<(int)vegetationVector.size();i++)
    {
        Vegetation* v = vegetationVector[i];

        delete v;
    }

    vegetationVector.clear();

    node->remove();

    //ocean->remove();///TODO: rever destrutor TerrainTile!

    selector->drop();
}

Vegetation* TerrainTile::getVegetationAt(vector3df pos)
{
    for (int i=0 ; i<(int)vegetationVector.size() ; i++)
    {
    	Vegetation* temp = (Vegetation*)vegetationVector[i];
    	if(temp->getPosition().getDistanceFrom(pos) < vegetationRange ) return temp;
    }
    return 0;
}

stringc TerrainTile::getName()
{
    return node->getName();
}

ISceneNode* TerrainTile::getNode()
{
    return node;
}

f32 TerrainTile::getNodeScale()
{
	return nodescale;
}
ITriangleSelector* TerrainTile::getTriangleSelector()
{
    return selector;
}

IMeshBuffer* TerrainTile::getMeshBuffer()
{
    return ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);
}

void TerrainTile::mergeToTile(TerrainTile* tile)
{
    if(!tile)
    {
        #ifdef APP_DEBUG
        cout << "DEBUG : TERRAIN TILE : MERGE FAILED, TILE IS NULL: " << endl;
        #endif
        return;
    }
    else
    {
        IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);
        IMeshBuffer* neighborBuffer = tile->getMeshBuffer();

        S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();
        S3DVertex* n_mb_vertices = (S3DVertex*) neighborBuffer->getVertices();

        u16* mb_indices  = meshBuffer->getIndices();
        u16* n_mb_indices  = neighborBuffer->getIndices();

        for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
        {
            for (unsigned int i = 0; i < neighborBuffer->getVertexCount(); i += 1)
            {
                vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
                vector3df nRealPos = n_mb_vertices[i].Pos*(scale/nodescale) + tile->getNode()->getPosition();

                if((realPos.X == nRealPos.X) && (realPos.Z == nRealPos.Z))
                {
                    mb_vertices[j].Pos.Y = n_mb_vertices[i].Pos.Y;
                }
            }
        }

        smgr->getMeshManipulator()->recalculateNormals(((IMeshSceneNode*)node)->getMesh(),true);
		((IMeshSceneNode*)node)->getMesh()->setDirty();
    }
}

void TerrainTile::saveToXML(TiXmlElement* parentElement)
{
	// Save the terrain land
    int x = (int)node->getPosition().X/(int)node->getScale().X;
    int z = (int)node->getPosition().Z/(int)node->getScale().Z;

    TiXmlElement* segmentXML = new TiXmlElement("terrainSegment");
    segmentXML->SetAttribute("x",x);
    segmentXML->SetAttribute("z",z);

    //write all vertex
    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);
	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();
	u16* mb_indices  = meshBuffer->getIndices();

    for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
	    if(realPos.Y != 0.0f )
        {
            TiXmlElement* vertexXML = new TiXmlElement("vertex");
            vertexXML->SetAttribute("id",j);
            vertexXML->SetAttribute("y",stringc(realPos.Y).c_str());
		
			Vegetation * tree = 0;
			for (int i=0 ; i<(int)vegetationVector.size() ; i++)
			{
    			Vegetation * temp = (Vegetation*)vegetationVector[i];
    			if(temp->getPosition().getDistanceFrom(realPos) < (vegetationRange/10) )
				{
					tree = temp;
					break;
				}
				else
					tree = 0;
			}
    			
			if (tree!=0)
			{
				vector3df treepos=tree->getPosition();
				vertexXML->SetAttribute("v",tree->getType());
				vertexXML->SetAttribute("tx",round32(tree->getPosition().X));
				vertexXML->SetAttribute("ty",round32(tree->getPosition().Y));
				vertexXML->SetAttribute("tz",round32(tree->getPosition().Z));
			}
			// vertexXML->SetAttribute("y",stringc((realPos.Y/(scale/nodescale))).c_str());
            segmentXML->LinkEndChild(vertexXML);
        }
	}

	parentElement->LinkEndChild(segmentXML);	
}

bool TerrainTile::loadFromXML(TiXmlElement* parentElement)
{
    TiXmlNode* vertex = parentElement->FirstChild( "vertex" );

    while( vertex != NULL )
    {
        s32 id = atoi(vertex->ToElement()->Attribute("id"));
        f32 y = (f32)atof(vertex->ToElement()->Attribute("y"));

		//int vegetation = atoi(vertex->ToElement()->Attribute("v"));
		stringc sttype = vertex->ToElement()->Attribute("v");
		if (sttype.size()>0)
		{
			int ttype=atoi(sttype.c_str());
			stringc stposx = vertex->ToElement()->Attribute("tx");
			if (stposx.size()>0)
			{
				f32 tposx = (f32)atof(stposx.c_str());
				f32 tposy = (f32)atof(vertex->ToElement()->Attribute("ty"));
				f32 tposz = (f32)atof(vertex->ToElement()->Attribute("tz"));

				// Now create a new tree with the informations
				Vegetation* v = new Vegetation(ttype);
				v->setPosition(vector3df(tposx,tposy,tposz));
				f32 treesize = (f32)(rand() % 20 + 50)/100;
				treesize*= 0.3f;
				v->setScale(vector3df(treesize*(scale/7.5f),treesize*(scale/7.5f),treesize*(scale/7.5f)));
				// Update the infos
				vegetationVector.push_back(v);
			}

		}
		this->transformMeshByVertex(id,y,false);

        vertex = parentElement->IterateChildren( "vertex", vertex );
    }
	return true;
}

void TerrainTile::paintVegetation(vector3df clickPos, bool erase)
{
    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	u16* mb_indices  = meshBuffer->getIndices();

	for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    if(erase)
        {
            vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
            clickPos.Y = realPos.Y;
            if(realPos.getDistanceFrom(clickPos) < vegetationRange/2 && getVegetationAt(vector3df(realPos.X,realPos.Y,realPos.Z)))
            {
                Vegetation* toRemove = getVegetationAt(vector3df(realPos.X,realPos.Y,realPos.Z));

                for (int i=0 ; i<(int)vegetationVector.size() ; i++)
                {
                    if(vegetationVector[i] == toRemove)
                    {
                        vegetationVector.erase(vegetationVector.begin() + i);
                        delete toRemove;

                        #ifdef APP_DEBUG
                        cout << "DEBUG : TERRAIN TILE : VEGETATION REMOVED: " << realPos.X << "," << realPos.X/(scale/nodescale) << "," << realPos.Z << "   TOTAL:" << vegetationVector.size() << endl;
                        #endif

                        break;
                    }
                }
            }
        }
        else
        {
            vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
            clickPos.Y = realPos.Y;
            if(realPos.getDistanceFrom(clickPos) < (vegetationRange/2) && !getVegetationAt(vector3df(realPos.X,realPos.Y,realPos.Z)))
            {
                Vegetation* v = new Vegetation();

                //v->setPosition(vector3df(realPos.X + (rand()%5)*0.1f - 0.25f,realPos.Y/(scale/nodescale),realPos.Z + (rand()%5)*0.1f - 0.25f));
				//v->setPosition(vector3df(realPos.X + (rand()%5)*scale/100,realPos.Y,realPos.Z + (rand()%5)*scale/100));
				v->setPosition(vector3df(realPos.X,realPos.Y,realPos.Z));
                f32 treesize = (f32)(rand() % 20 + 50)/100;
                treesize*= 0.3f;
				v->setScale(vector3df(treesize*(scale/7.5f),treesize*(scale/7.5f),treesize*(scale/7.5f)));

				printf("Attempting to place a tree with this size: %f\n",treesize);
                vegetationVector.push_back(v);

                #ifdef APP_DEBUG
                cout << "DEBUG : TERRAIN TILE : VEGETATION CREATED: " << realPos.X << "," << realPos.Y << "," << realPos.Z << "   TOTAL:" << vegetationVector.size() << endl;
                #endif
	        }
	    }
	}
}

void TerrainTile::transformMeshByVertex(s32 id, f32 y, bool addVegetation)
{
	IVideoDriver * driver = smgr->getGUIEnvironment()->getVideoDriver();


    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	u16* mb_indices  = meshBuffer->getIndices();

	mb_vertices[id].Pos.Y = y;
	driver->draw3DBox(aabbox3d<f32>(vector3df(mb_vertices[id].Pos.X-10,mb_vertices[id].Pos.Y-10,mb_vertices[id].Pos.Z-10),
		vector3df(mb_vertices[id].Pos.X+10,mb_vertices[id].Pos.Y+10,mb_vertices[id].Pos.Z+10)),video::SColor(255,255,255,255));
		//aabbox3d((f32),(f32),(f32),
		//(f32),(f32),(f32)),video::SColor(255,255,255,255));

	if(addVegetation)
	{
	    vector3df treePos = vector3df(mb_vertices[id].Pos.X,y , mb_vertices[id].Pos.Z);
	    treePos *= node->getScale();
	    treePos.Y = y;
	    treePos += node->getPosition();
	    paintVegetation(treePos ,false);
	}

	smgr->getMeshManipulator()->recalculateNormals(((IMeshSceneNode*)node)->getMesh(),true);

	// Attempt to update the triangle selector with the new mesh
	ITriangleSelector * selector = smgr->createTriangleSelector(((IMeshSceneNode*)node)->getMesh(),node);
    node->setTriangleSelector(selector);
	selector->drop();
}


void TerrainTile::transformMesh(vector3df clickPos, f32 radius, f32 strength)
{
    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	u16* mb_indices  = meshBuffer->getIndices();

	for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
        clickPos.Y = realPos.Y;

	    if(realPos.getDistanceFrom(clickPos) < radius)
	    {
            //f32 ratio = sin(radius - realPos.getDistanceFrom(clickPos));
			f32 ratio = radius - realPos.getDistanceFrom(clickPos);
	        mb_vertices[j].Pos.Y += (strength * (ratio)/(scale/nodescale));
			//printf("found something here: vertice %i, vertice Y: %f\n",j, mb_vertices[j].Pos.Y);
	    }

		if(mb_vertices[j].Pos.Y > nodescale/4) mb_vertices[j].Pos.Y = nodescale/4;
	    if(mb_vertices[j].Pos.Y < -(nodescale*0.1f)) mb_vertices[j].Pos.Y = -(nodescale*0.1f);
	}


	smgr->getMeshManipulator()->recalculateNormals(((IMeshSceneNode*)node)->getMesh(),true);
	// Attempt to update the triangle selector with the new mesh
	ITriangleSelector * selector = smgr->createTriangleSelector(((IMeshSceneNode*)node)->getMesh(),node);
    node->setTriangleSelector(selector);
	selector->drop();
	
	((IMeshSceneNode*)node)->getMesh()->setDirty();
}

void TerrainTile::transformMeshToValue(vector3df clickPos, f32 radius, f32 strength, f32 value)
{
    if(strength<0) strength = -strength;

    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	u16* mb_indices  = meshBuffer->getIndices();

	for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
        clickPos.Y = realPos.Y;
	    if(realPos.getDistanceFrom(clickPos) < radius)
	    {
            //f32 ratio = sin(radius - realPos.getDistanceFrom(clickPos));
			f32 ratio = radius - realPos.getDistanceFrom(clickPos);

            if(mb_vertices[j].Pos.Y > value)
            {
				//mb_vertices[j].Pos.Y -= strength * ratio / (scale/nodescale);
                mb_vertices[j].Pos.Y -= strength * ratio / (scale/nodescale);
                if(mb_vertices[j].Pos.Y <= value) mb_vertices[j].Pos.Y = value;

            }
            if(mb_vertices[j].Pos.Y < value)
            {
		        //mb_vertices[j].Pos.Y += strength * ratio / (scale / nodescale);
				mb_vertices[j].Pos.Y += strength *ratio / (scale / nodescale);
                if(mb_vertices[j].Pos.Y >= value) mb_vertices[j].Pos.Y = value;
            }
	    }
	}

	smgr->getMeshManipulator()->recalculateNormals(((IMeshSceneNode*)node)->getMesh(),true);
	// Attempt to update the triangle selector with the new mesh
	ITriangleSelector * selector = smgr->createTriangleSelector(((IMeshSceneNode*)node)->getMesh(),node);
    node->setTriangleSelector(selector);
	selector->drop();
	((IMeshSceneNode*)node)->getMesh()->setDirty();
}

void TerrainTile::transformMeshDOWN(vector3df clickPos, f32 radius, f32 strength)
{

	printf ("this was called to down\n");
    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	u16* mb_indices  = meshBuffer->getIndices();

	for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();
        clickPos.Y = realPos.Y;
	    if(realPos.getDistanceFrom(clickPos) < radius)
	    {
            //f32 ratio = sin(radius - realPos.getDistanceFrom(clickPos));
			f32 ratio = radius - realPos.getDistanceFrom(clickPos);
	        mb_vertices[j].Pos.Y -= strength * ratio / (scale/nodescale);
	    }

	    //if(mb_vertices[j].Pos.Y < 0 ) mb_vertices[j].Pos.Y = 0;
	    cout << mb_vertices[j].Pos.Y << endl;
	}

	smgr->getMeshManipulator()->recalculateNormals(((IMeshSceneNode*)node)->getMesh(),true);
	// Attempt to update the triangle selector with the new mesh
	ITriangleSelector * selector = smgr->createTriangleSelector(((IMeshSceneNode*)node)->getMesh(),node);
    node->setTriangleSelector(selector);
	selector->drop();
	((IMeshSceneNode*)node)->getMesh()->setDirty();
}

f32 TerrainTile::getHeightAt(vector3df pos)
{
	scene::ISceneCollisionManager* collMan = smgr->getSceneCollisionManager();
	core::line3d<f32> ray;
    ray.start = pos+vector3df(0,+250,0);
    ray.end = ray.start + (pos+vector3df(0,-250.0f,0) - ray.start).normalize() * 1000.0f;

	// Tracks the current intersection point with the level or a mesh
	core::vector3df intersection;
    // Used to show with triangle has been hit
    core::triangle3df hitTriangle;
	scene::ISceneNode * selectedSceneNode =
    collMan->getSceneNodeAndCollisionPointFromRay(
		ray,
		intersection,
		hitTriangle,
		100,
		0); // Check the entire scene (this is actually the implicit default)

	if (selectedSceneNode)
	{
		//printf("Test was successful! value=%f ,%s\n",intersection.Y,selectedSceneNode->getName());
		return intersection.Y;
	}
	else
		return 0;

	// This stuff is not used anymore.. Keep for the moment in case of problems...
    IMeshBuffer* meshBuffer = ((IMeshSceneNode*)node)->getMesh()->getMeshBuffer(0);

	S3DVertex* mb_vertices = (S3DVertex*) meshBuffer->getVertices();

	f32 height = 0;
	vector3df nearestVertex = vector3df(1000,1000,1000);

	for (unsigned int j = 0; j < meshBuffer->getVertexCount(); j += 1)
	{
	    vector3df realPos = mb_vertices[j].Pos*(scale/nodescale) + node->getPosition();

	    if(pos.getDistanceFrom(vector3df(realPos.X,0,realPos.Z) ) < pos.getDistanceFrom( vector3df(nearestVertex.X,0,nearestVertex.Z) ))
	    {
	        nearestVertex = realPos;
	        height = realPos.Y;
	    }
	}

	return height;
}

void TerrainTile::showDebugData(bool show)
{
    for(int i=0;i<(int)vegetationVector.size();i++)
    {
        vegetationVector[i]->showDebugData(show);
    }
}
