// Copyright (C) 2002-2006 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
//
// This file was originally written by ZDimitor.

//--------------------------------------------------------------------------------
// This tool created by ZDimitor everyone can use it as wants
//--------------------------------------------------------------------------------

#include "CMY3DMeshFileLoader.h"

#include "SAnimatedMesh.h"
#include "SMeshBuffer.h"
#include "IAttributes.h"


#include "CMY3DStuff.h"
#include "CMY3DHelper.h"
#include "os.h"

// v3.15 - May 16, 2005

namespace irr
{
namespace scene
{


CMY3DMeshFileLoader::CMY3DMeshFileLoader(
	io::IFileSystem* fs, video::IVideoDriver* driver, ISceneManager *scmgr)
	: Mesh(0), Driver(driver), FileSystem(fs), SceneManager(scmgr)
{
	if (Driver)
		Driver->grab();

	if (FileSystem)
		FileSystem->grab();
}



CMY3DMeshFileLoader::~CMY3DMeshFileLoader()
{
	if (Mesh)
		Mesh->drop();

	if (Driver)
		Driver->drop();

	if (FileSystem)
		FileSystem->drop();
}



bool CMY3DMeshFileLoader::isALoadableFileExtension(const c8* filename)
{
    return strstr(filename, ".my3d") != 0;
}


IAnimatedMesh* CMY3DMeshFileLoader::createMesh(io::IReadFile* file)
{
	MaterialEntry.clear();
	MeshBufferEntry.clear();
	ChildNodes.clear();

	core::stringc file_name = file->getFileName();

	// working directory (from wich we loading the scene)
	c8 WorkDir[1024];
	core::extractFilePath((c8*)file_name.c_str(), (c8*)WorkDir, 1024);

	core::stringc msg="";

	//msg="Loading 3d data from ";
	//msg.append(file_name);
	//os::Printer::log(msg.c_str(), ELL_INFORMATION);

	// read file into memory
	u16 id;
	c8 name[256];

	SMyFileHeader fileHeader;
	file->read(&fileHeader, sizeof(SMyFileHeader));

	if (fileHeader.MyId!=MY_ID || fileHeader.Ver!=MY_VER)
	{
		os::Printer::log("Bad MY3D file header, loading failed!", ELL_ERROR);
		return 0;
	}

	file->read(&id, sizeof(id));

	if (id!=MY_SCENE_HEADER_ID)
	{
		os::Printer::log("Can not find MY_SCENE_HEADER_ID, loading failed!", ELL_ERROR);
		return 0;
	}

	SMySceneHeader sceneHeader;
	file->read(&sceneHeader, sizeof(SMySceneHeader));

	SceneBackgrColor = video::SColor(
		sceneHeader.BackgrColor.R, sceneHeader.BackgrColor.G,
		sceneHeader.BackgrColor.B, sceneHeader.BackgrColor.A);

	SceneAmbientColor = video::SColor(
		sceneHeader.AmbientColor.R, sceneHeader.AmbientColor.G,
		sceneHeader.AmbientColor.B, sceneHeader.AmbientColor.A);

	file->read(&id, sizeof(id));

	if (id!=MY_MAT_LIST_ID)
	{
		os::Printer::log("Can not find MY_MAT_LIST_ID, loading failed!", ELL_ERROR);
		return 0;
	}


	// loading materials and textures
	//c8 ch[255];
	//sprintf(ch, "Loading materials (%d to go) and textures...", sceneHeader.MaterialCount);
	//os::Printer::log(ch, ELL_INFORMATION);

	core::stringc texturePath =
		SceneManager->getParameters()->getAttributeAsString(MY3D_TEXTURE_PATH);

	s32 texCount=0, ligCount=0, matCount=0;

    file->read(&id, sizeof(id));

    int m;
    for (m=0; m<sceneHeader.MaterialCount; m++)
    {
		if (id!=MY_MAT_HEADER_ID)
        {   os::Printer::log("Can not find MY_MAT_HEADER_ID, loading failed!", ELL_ERROR);
		    return 0;
        }

		matCount++;

		SMyMaterialEntry me;

		// read material header
		SMyMaterialHeader materialHeader;
		file->read(&materialHeader, sizeof(SMyMaterialHeader));
		me.Header = materialHeader;

		// read next identificator
		file->read(&id, sizeof(id));

		bool GetLightMap=false, GetMainMap=false;
		static int LightMapIndex=0;

        for (int t=0; t<materialHeader.TextureCount; t++)
        {
            if (id==MY_TEX_FNAME_ID)
                file->read(name, 256);
            else
            {
                file->read(&id, sizeof(id));
                if (id!=MY_TEXDATA_HEADER_ID)
                {   os::Printer::log("Can not find MY_TEXDATA_HEADER_ID, loading failed!", ELL_ERROR);
		            return 0;
                }

                SMyTexDataHeader texDataHeader;

                file->read(&texDataHeader, sizeof(SMyTexDataHeader));

                core::strcpy(texDataHeader.Name, name);

                char LightMapName[255];
                sprintf(LightMapName,"My3D.Lightmap.%d",++LightMapIndex);

                core::stringc pixFormatStr;
                if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_24)
					pixFormatStr = "24bit,";
                else
				if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_16)
					pixFormatStr = "16bit,";
                else
                {
					msg="Unknown format of image data (";
                    msg.append(LightMapName);
                    msg.append("), loading failed!");
                    os::Printer::log(msg.c_str(), ELL_ERROR);
                    return 0;
                }

				if (texDataHeader.ComprMode != MY_TEXDATA_COMPR_NONE_ID &&
					texDataHeader.ComprMode != MY_TEXDATA_COMPR_RLE_ID &&
					texDataHeader.ComprMode != MY_TEXDATA_COMPR_SIMPLE_ID )
				{
					os::Printer::log("Unknown method of compression image data, loading failed!", ELL_ERROR);
		            return 0;
                }


                s32 num_pixels = texDataHeader.Width*texDataHeader.Height;

                void* data = 0;

                if (texDataHeader.ComprMode==MY_TEXDATA_COMPR_NONE_ID)
                {
					// none compressed image data
                    if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_24)
                    {
						data = (void*) new SMyPixelColor24[num_pixels];
                        file->read(data, sizeof(SMyPixelColor24)*num_pixels);
                    }
                    else
                    {
						data = (void*) new SMyPixelColor16[num_pixels];
                        file->read(data, sizeof(SMyPixelColor16)*num_pixels);
                    }
                }
				else
				if (texDataHeader.ComprMode==MY_TEXDATA_COMPR_RLE_ID)
				{
					// read RLE header identificator
					file->read(&id, sizeof(id));
					if (id!=MY_TEXDATA_RLE_HEADER_ID)
					{
						os::Printer::log("Can not find MY_TEXDATA_RLE_HEADER_ID, loading failed!", ELL_ERROR);
						return 0;
					}

					// read RLE header
					SMyRLEHeader rleHeader;
					file->read(&rleHeader, sizeof(SMyRLEHeader));

					//allocate memory for input and output buffers
					void *input_buffer  = (void*) new unsigned char[rleHeader.nEncodedBytes];
					void *output_buffer = (void*) new unsigned char[rleHeader.nDecodedBytes];

					// read encoded data
					file->read(input_buffer, rleHeader.nEncodedBytes);

					// decode data
					data = 0;//(void*) new unsigned char[rleHeader.nDecodedBytes];
                    s32 decodedBytes = core::rle_decode(
						(unsigned char*)input_buffer,  rleHeader.nEncodedBytes,
						(unsigned char*)output_buffer, rleHeader.nDecodedBytes);

                    if (decodedBytes!=(s32)rleHeader.nDecodedBytes)
                    {
						os::Printer::log("Error extracting data from RLE compression, loading failed!", ELL_ERROR);
						return 0;
                    }

					// free input buffer
					delete [] (unsigned char*)input_buffer;

					// here decoded data
					data = output_buffer;

				}
                else if (texDataHeader.ComprMode==MY_TEXDATA_COMPR_SIMPLE_ID)
                {
					// simple compressed image data
                    if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_24)
                        data = (void*) new SMyPixelColor24[num_pixels];
                    else
                        data = (void*) new SMyPixelColor16[num_pixels];

                    u32 nReadedPixels =0, nToRead =0;
                    while (true)
                    {
                        file->read(&nToRead, sizeof(nToRead));

                        if ((s32)(nReadedPixels+nToRead)>(s32)num_pixels) break;

                        if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_24)
                        {
							SMyPixelColor24 col24;
                            file->read(&col24, sizeof(SMyPixelColor24));
for (u32 p=0; p<nToRead; p++)
                            {   ((SMyPixelColor24*)data)[nReadedPixels+p] =
                                    SMyPixelColor24(col24.r, col24.g, col24.b);
                            }
                        }
                        else
                        {
							SMyPixelColor16 col16;
                            file->read(&col16, sizeof(SMyPixelColor16));
                            for (u32 p=0; p<nToRead; p++)
                            {   ((SMyPixelColor16*)data)[nReadedPixels+p].argb = col16.argb;
                            }
                        }

                        nReadedPixels+=nToRead;

                        if ((s32)nReadedPixels>=(s32)num_pixels) break;
                    }

                    if ((s32)nReadedPixels!=(s32)num_pixels)
                    {
                        os::Printer::log("Image data seems to be corrupted, loading failed!", ELL_ERROR);
		                return 0;
                    }
                }

                //! Creates a software image from a byte array.
                video::IImage* light_img = 0;

                if (texDataHeader.PixelFormat == MY_PIXEL_FORMAT_24)
                {
					// 24 bit lightmap format
                    light_img = Driver->createImageFromData(
                        video::ECF_R8G8B8, 
                        core::dimension2d<s32>(texDataHeader.Width, texDataHeader.Height), 
                        data, true);
                }
				else
                {
					// 16 bit lightmap format
                    light_img = Driver->createImageFromData(
                        video::ECF_A1R5G5B5, 
                        core::dimension2d<s32>(texDataHeader.Width, texDataHeader.Height), 
                        data, true);
                }

				bool oldMipMapState = Driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
				Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

                me.Texture2 = Driver->addTexture(LightMapName, light_img);
				ligCount++;

				Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, oldMipMapState);

                light_img->drop();

                GetLightMap = true;
            }

			core::stringc Name = name;
            int pos2 = Name.findLast('.');
            core::stringc  LightingMapStr = "LightingMap";
            int ls = LightingMapStr.size();
            core::stringc sub = Name.subString((s32)core::fmax((f32)0, (f32)(pos2 - ls)), ls);
            core::stringc texFName;

            if ((sub == LightingMapStr || (Name[pos2-1]=='m' && 
				 Name[pos2-2]=='l' && Name[pos2-3]=='_')) &&
				!GetLightMap)
			{
				bool oldMipMapState = Driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
				Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

				texFName = texturePath.size() ? texturePath : WorkDir;
                texFName.append("Lightmaps/"); 
				texFName.append(Name);
				me.Texture2FileName = texFName;

                if (Name.size()>0)
				{
                    me.Texture2 = Driver->getTexture(texFName.c_str());
					ligCount++;
				}

                GetLightMap = true;

				me.MaterialType = video::EMT_LIGHTMAP_M2;

				Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, oldMipMapState);
			}
			else 
			if (!GetLightMap&&GetMainMap)
			{
				texFName = texturePath.size() ? texturePath : WorkDir;
				texFName.append(Name);
				me.Texture2FileName = texFName;

                if (Name.size()>0)
				{
                    me.Texture2 = Driver->getTexture(texFName.c_str());
					ligCount++;
				}

				me.MaterialType = video::EMT_REFLECTION_2_LAYER;
			}
			else 
			if (!GetMainMap && !GetLightMap )
			{
				texFName = WorkDir;
				texFName.append(Name);
				me.Texture1FileName = texFName;
                if (Name.size()>0)
				{
                    			me.Texture1 = Driver->getTexture(texFName.c_str());
					texCount++;
				}

                GetMainMap = true;
				me.MaterialType = video::EMT_SOLID;
			}
			else 
			if (GetLightMap)
			{
				me.MaterialType = video::EMT_LIGHTMAP_M2;
			}

			file->read(&id, sizeof(id));
		}

		// override materials types from they names

		if (me.Header.Name[0] =='A' &&
			me.Header.Name[1] =='l' &&
			me.Header.Name[2] =='p' &&
			me.Header.Name[3] =='h' &&
			me.Header.Name[4] =='a' &&
			me.Header.Name[5] =='C' &&
			me.Header.Name[6] =='h' &&
			me.Header.Name[7] =='a' &&
			me.Header.Name[8] =='n' &&
			me.Header.Name[9] =='n' &&
			me.Header.Name[10]=='e' &&
			me.Header.Name[11]=='l' &&
			me.Header.Name[12]=='-'
			)
		{
			me.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		}
		else
		if (me.Header.Name[0] =='S' &&
			me.Header.Name[1] =='p' &&
			me.Header.Name[2] =='h' &&
			me.Header.Name[3] =='e' &&
			me.Header.Name[4] =='r' &&
			me.Header.Name[5] =='e' &&
			me.Header.Name[6] =='M' &&
			me.Header.Name[7] =='a' &&
			me.Header.Name[8] =='p' &&
			me.Header.Name[9] =='-'
			)
		{
			me.MaterialType = video::EMT_SPHERE_MAP;
		}

		MaterialEntry.push_back(me);
	}

	// loading meshes

	//sprintf(ch, " Loaded: materials (%d), textures (%d), lightmaps (%d).", matCount, texCount, ligCount);
	// os::Printer::log(ch, ELL_INFORMATION);

	if (Mesh) 
		Mesh->drop();

	Mesh = new SMesh();   

	//sprintf(ch, "Loading meshes (%d to go) ...", sceneHeader.MeshCount);
	//os::Printer::log(ch, ELL_INFORMATION);

	if (id!=MY_MESH_LIST_ID)
	{
		os::Printer::log("Can not find MY_MESH_LIST_ID, loading failed!", ELL_ERROR);
		return 0;
	}

	file->read(&id, sizeof(id));
	   
    for (s32 mesh_id=0; mesh_id<sceneHeader.MeshCount; mesh_id++)
	{
        // Warning!!! In some cases MY3D exporter uncorrectly calculates
        // MeshCount (it's a problem, has to be solved) thats why 
        // i added this code line
        if (id!=MY_MESH_HEADER_ID) 
			break;

        if (id!=MY_MESH_HEADER_ID)
    	{
			os::Printer::log("Can not find MY_MESH_HEADER_ID, loading failed!", ELL_ERROR);
		    return 0;
	    }

		SMyMeshHeader meshHeader;
		file->read(&meshHeader, sizeof(SMyMeshHeader));

		core::array <SMyVertex> Vertex;
		core::array <SMyFace> Face;
		core::array <SMyTVertex> TVertex1, TVertex2;
		core::array <SMyFace> TFace1, TFace2;

		s32 vertsNum=0;
		s32 facesNum=0;

		// verticies
		file->read(&id, sizeof(id));
		if (id!=MY_VERTS_ID)
		{
			os::Printer::log("Can not find MY_VERTS_ID, loading failed!", ELL_ERROR);
			return 0;
		}

		file->read(&vertsNum, sizeof(vertsNum));
		Vertex.reallocate(vertsNum);
		file->read(Vertex.pointer(), sizeof(SMyVertex)*vertsNum);
        Vertex.set_used(vertsNum);

		// faces
		file->read(&id, sizeof(id));
		if (id!=MY_FACES_ID)
		{
			os::Printer::log("Can not find MY_FACES_ID, loading failed!", ELL_ERROR);
			return 0;
		}

		file->read(&facesNum, sizeof(facesNum));
		Face.reallocate(facesNum);
		file->read(Face.pointer(), sizeof(SMyFace)*facesNum);
        Face.set_used(facesNum);

        // reading texture channels
        for (s32 tex=0; tex<(s32)meshHeader.TChannelCnt; tex++)
        {
			// Max 2 texture channels allowed (but in format .my3d can be more)
			s32 tVertsNum=0, tFacesNum=0;

            // reading texture coords
            file->read(&id, sizeof(id));

	    	if (id!=MY_TVERTS_ID)
		    {   msg="Can not find MY_TVERTS_ID (";
                msg.append(tex);
                msg.append("texture channel), loading failed!");
                os::Printer::log(msg.c_str(), ELL_ERROR);
    			return 0;
	    	}

    		file->read(&tVertsNum, sizeof(tVertsNum));

            if (tex==0)
            {
				// 1st texture channel
				TVertex1.reallocate(tVertsNum);
		        file->read(TVertex1.pointer(), sizeof(SMyTVertex)*tVertsNum);
                TVertex1.set_used(tVertsNum);
            }
            else
			if (tex==1)
            {
				// 2nd texture channel
				TVertex2.reallocate(tVertsNum);
		        file->read(TVertex2.pointer(), sizeof(SMyTVertex)*tVertsNum);
                TVertex2.set_used(tVertsNum);
            }
            else
            {
				// skip other texture channels
				u32 pos = file->getPos();
                file->seek(pos+sizeof(SMyTVertex)*tVertsNum);
            }

            // reading texture faces
		    file->read(&id, sizeof(id));

		    if (id!=MY_TFACES_ID)
            {   msg="Can not find MY_TFACES_ID (";
                msg.append(tex);
                msg.append("texture channel), loading failed!");
                os::Printer::log(msg.c_str(), ELL_ERROR);
    			return 0;
	    	}

		    file->read(&tFacesNum, sizeof(tFacesNum));

            if (tex==0)
            {
				// 1st texture channel
				TFace1.reallocate(tFacesNum);
		        file->read(TFace1.pointer(), sizeof(SMyFace)*tFacesNum);
                TFace1.set_used(tFacesNum);
            }
            else if (tex==1)
            {
				// 2nd texture channel
				TFace2.reallocate(tFacesNum);
		        file->read(TFace2.pointer(), sizeof(SMyFace)*tFacesNum);
                TFace2.set_used(tFacesNum);
            }
            else
            {
				// skip other texture channels
				u32 pos = file->getPos();
                file->seek(pos+sizeof(SMyFace)*tFacesNum);
            }
        }

		// trying to find material

		SMyMaterialEntry* matEnt = getMaterialEntryByIndex(meshHeader.MatIndex);

		// creating geometry for the mesh

		// trying to find mesh buffer for this material
		SMeshBufferLightMap* buffer = getMeshBufferByMaterialIndex(meshHeader.MatIndex);

		if (!buffer ||
			(buffer->Vertices.size()+vertsNum) > Driver->getMaximalPrimitiveCount())
		{
			// creating new mesh buffer for this material
			buffer = new scene::SMeshBufferLightMap();

			buffer->Material.MaterialType = video::EMT_LIGHTMAP_M2; // EMT_LIGHTMAP_M4 also possible
			buffer->Material.Wireframe = false;
			buffer->Material.Lighting = false;
			buffer->Material.BilinearFilter = true;

			if (matEnt)
			{
				buffer->Material.MaterialType = matEnt->MaterialType;

				if (buffer->Material.MaterialType == video::EMT_REFLECTION_2_LAYER)
				{
					buffer->Material.Lighting = true;
					buffer->Material.Textures[1] = matEnt->Texture1;
					buffer->Material.Textures[0] = matEnt->Texture2;
				}
				else
				{
					buffer->Material.Textures[0] = matEnt->Texture1;
					buffer->Material.Textures[1] = matEnt->Texture2;
				}

				if (buffer->Material.MaterialType == video::EMT_TRANSPARENT_ALPHA_CHANNEL)
				{
					buffer->Material.BackfaceCulling = true;
					buffer->Material.Lighting  = true;
				}
				else
				if (buffer->Material.MaterialType == video::EMT_SPHERE_MAP)
				{
					buffer->Material.Lighting  = true;
				}

				buffer->Material.AmbientColor = video::SColor(
					matEnt->Header.AmbientColor.A, matEnt->Header.AmbientColor.R,
					matEnt->Header.AmbientColor.G, matEnt->Header.AmbientColor.B
					);
				buffer->Material.DiffuseColor =	video::SColor(
					matEnt->Header.DiffuseColor.A, matEnt->Header.DiffuseColor.R,
					matEnt->Header.DiffuseColor.G, matEnt->Header.DiffuseColor.B
					);
				buffer->Material.EmissiveColor = video::SColor(
					matEnt->Header.EmissiveColor.A, matEnt->Header.EmissiveColor.R,
					matEnt->Header.EmissiveColor.G, matEnt->Header.EmissiveColor.B
					);
				buffer->Material.SpecularColor = video::SColor(
					matEnt->Header.SpecularColor.A, matEnt->Header.SpecularColor.R,
					matEnt->Header.SpecularColor.G, matEnt->Header.SpecularColor.B
					);
			}
			else
	        {
			buffer->Material.Textures[0] = 0;
		        buffer->Material.Textures[1] = 0;

			buffer->Material.AmbientColor = video::SColor(255, 255, 255, 255);
			buffer->Material.DiffuseColor =	video::SColor(255, 255, 255, 255);
			buffer->Material.EmissiveColor = video::SColor(0, 0, 0, 0);
			buffer->Material.SpecularColor = video::SColor(0, 0, 0, 0);
	        }

			if (matEnt && matEnt->Header.Transparency!=0)
			{
				if (buffer->Material.MaterialType == video::EMT_REFLECTION_2_LAYER )
				{
					buffer->Material.MaterialType = video::EMT_TRANSPARENT_REFLECTION_2_LAYER;
					buffer->Material.Lighting  = true;
					buffer->Material.BackfaceCulling = true;
				}
				else
				{
					buffer->Material.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
					buffer->Material.Lighting = false;
					buffer->Material.BackfaceCulling = false;
				}
	        }
			else if (
				!buffer->Material.Textures[1] &&
				buffer->Material.MaterialType != video::EMT_TRANSPARENT_ALPHA_CHANNEL &&
				buffer->Material.MaterialType != video::EMT_SPHERE_MAP
				)
	        {
		        buffer->Material.MaterialType = video::EMT_SOLID;
				buffer->Material.Lighting  = true;
			}

			MeshBufferEntry.push_back(
			SMyMeshBufferEntry(meshHeader.MatIndex, buffer));
		}

		video::S3DVertex2TCoords VertexA, VertexB, VertexC;
		video::SColor vert_color;
		core::triangle3df face;

		for (int f=0; f<facesNum; f++)
		{
			// vertices (A, B, C) color

			if (matEnt &&
				(buffer->Material.MaterialType == video::EMT_TRANSPARENT_VERTEX_ALPHA ||
				buffer->Material.MaterialType == video::EMT_TRANSPARENT_REFLECTION_2_LAYER))
			{
				video::SColor color(
				matEnt->Header.DiffuseColor.A, matEnt->Header.DiffuseColor.R,
				matEnt->Header.DiffuseColor.G, matEnt->Header.DiffuseColor.B);

				vert_color = color.getInterpolated(video::SColor(0,0,0,0),
					1-matEnt->Header.Transparency);
			}
			else
			{
				vert_color = buffer->Material.DiffuseColor;
			}

			VertexA.Color = VertexB.Color = VertexC.Color = vert_color;

			// vertex A

			VertexA.Pos.X = Vertex[Face[f].C].Coord.X;
			VertexA.Pos.Y = Vertex[Face[f].C].Coord.Y;
			VertexA.Pos.Z = Vertex[Face[f].C].Coord.Z;

			VertexA.Normal.X = Vertex[Face[f].C].Normal.X;
			VertexA.Normal.Y = Vertex[Face[f].C].Normal.Y;
			VertexA.Normal.Z = Vertex[Face[f].C].Normal.Z;

			if (meshHeader.TChannelCnt>0)
			{
				VertexA.TCoords.X  = TVertex1[TFace1[f].C].TCoord.X;
				VertexA.TCoords.Y  = TVertex1[TFace1[f].C].TCoord.Y;
			}

			if (meshHeader.TChannelCnt>1)
			{
				VertexA.TCoords2.X = TVertex2[TFace2[f].C].TCoord.X;
				VertexA.TCoords2.Y = TVertex2[TFace2[f].C].TCoord.Y;
			}

			// vertex B

			VertexB.Pos.X = Vertex[Face[f].B].Coord.X;
			VertexB.Pos.Y = Vertex[Face[f].B].Coord.Y;
			VertexB.Pos.Z = Vertex[Face[f].B].Coord.Z;

			VertexB.Normal.X = Vertex[Face[f].B].Normal.X;
			VertexB.Normal.Y = Vertex[Face[f].B].Normal.Y;
			VertexB.Normal.Z = Vertex[Face[f].B].Normal.Z;

			if (meshHeader.TChannelCnt>0)
			{
				VertexB.TCoords.X  = TVertex1[TFace1[f].B].TCoord.X;
				VertexB.TCoords.Y  = TVertex1[TFace1[f].B].TCoord.Y;
			}

			if (meshHeader.TChannelCnt>1)
			{
				VertexB.TCoords2.X = TVertex2[TFace2[f].B].TCoord.X;
				VertexB.TCoords2.Y = TVertex2[TFace2[f].B].TCoord.Y;
			}

			// vertex C

			VertexC.Pos.X = Vertex[Face[f].A].Coord.X;
			VertexC.Pos.Y = Vertex[Face[f].A].Coord.Y;
			VertexC.Pos.Z = Vertex[Face[f].A].Coord.Z;

			VertexC.Normal.X = Vertex[Face[f].A].Normal.X;
			VertexC.Normal.Y = Vertex[Face[f].A].Normal.Y;
			VertexC.Normal.Z = Vertex[Face[f].A].Normal.Z;

			if (meshHeader.TChannelCnt>0)
			{
				VertexC.TCoords.X  = TVertex1[TFace1[f].A].TCoord.X;
				VertexC.TCoords.Y  = TVertex1[TFace1[f].A].TCoord.Y;
			}
			if (meshHeader.TChannelCnt>1)
			{
				VertexC.TCoords2.X = TVertex2[TFace2[f].A].TCoord.X;
				VertexC.TCoords2.Y = TVertex2[TFace2[f].A].TCoord.Y;
			}

			// store 3d data in mesh buffer

			buffer->Indices.push_back(buffer->Vertices.size());
			buffer->Vertices.push_back(VertexA);

			buffer->Indices.push_back(buffer->Vertices.size());
			buffer->Vertices.push_back(VertexB);

			buffer->Indices.push_back(buffer->Vertices.size());
			buffer->Vertices.push_back(VertexC);

			//*****************************************************************
			//          !!!!!! W A R N I N G !!!!!!!
			//*****************************************************************
			// For materials with alpha channel we duplicate all faces.
			// This has be done for proper lighting calculation of the back faces.
			// So you must remember this while you creating your models !!!!!
			//*****************************************************************
			//          !!!!!! W A R N I N G !!!!!!!
			//*****************************************************************

			if (buffer->Material.MaterialType == video::EMT_TRANSPARENT_ALPHA_CHANNEL)
			{
				VertexA.Normal = core::vector3df(-VertexA.Normal.X, -VertexA.Normal.Y, -VertexA.Normal.Z);
				VertexB.Normal = core::vector3df(-VertexB.Normal.X, -VertexB.Normal.Y, -VertexB.Normal.Z);
				VertexC.Normal = core::vector3df(-VertexC.Normal.X, -VertexC.Normal.Y, -VertexC.Normal.Z);

				buffer->Indices.push_back(buffer->Vertices.size());
				buffer->Vertices.push_back(VertexC);

				buffer->Indices.push_back(buffer->Vertices.size());
				buffer->Vertices.push_back(VertexB);

				buffer->Indices.push_back(buffer->Vertices.size());
				buffer->Vertices.push_back(VertexA);
			}
		}
		file->read(&id, sizeof(id));
	}

	// creating mesh

	for (m=0; m<(s32)MeshBufferEntry.size(); m++)
	{
		SMeshBufferLightMap* buffer = MeshBufferEntry[m].MeshBuffer;

		if (!buffer)
			continue;

		Mesh->addMeshBuffer(buffer);

		buffer->recalculateBoundingBox();
		buffer->drop();
	}

	Mesh->recalculateBoundingBox();

	//sprintf(ch, "Loaded: meshes (%d).", MeshBufferEntry.size());
	//os::Printer::log(ch, ELL_INFORMATION);

	if (id != MY_FILE_END_ID)
		os::Printer::log("Loading finished, but can not find MY_FILE_END_ID token.", ELL_WARNING);

	SAnimatedMesh* am = new SAnimatedMesh();

	// you have to add this type in IAnimatedMesh.h
	//am->Type = EAMT_MY3D;

	am->addMesh(Mesh);
	am->recalculateBoundingBox();

	Mesh->drop();
	Mesh = 0;

	//msg="3D data successfully loaded from ";
	//msg.append(file_name);
	//os::Printer::log(msg.c_str(), ELL_INFORMATION);

	return am;
}


CMY3DMeshFileLoader::SMyMaterialEntry* CMY3DMeshFileLoader::getMaterialEntryByIndex(u32 matInd)
{
	for (int m=0; m<(s32)MaterialEntry.size(); m++)
		if (MaterialEntry[m].Header.Index == matInd)
			return &MaterialEntry[m];

	return NULL;
}



SMeshBufferLightMap* CMY3DMeshFileLoader::getMeshBufferByMaterialIndex(u32 matInd)
{
    for (int m=0; m<(s32)MeshBufferEntry.size(); m++)
	{
        if (MeshBufferEntry[m].MaterialIndex == (s32)matInd)
			return MeshBufferEntry[m].MeshBuffer;
	}
	return NULL;
}



core::array<ISceneNode*>& CMY3DMeshFileLoader::getChildNodes()
{
	return ChildNodes;
}


} // end namespace scnene
} // end namespace irr
