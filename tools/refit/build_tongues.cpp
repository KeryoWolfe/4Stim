// Converts OStim's tongues (Skyrim SE, by Egilvar) to Fallout 4 for 4Stim's
// equip objects: each shape moved from Skyrim's head to Fallout 4's (the
// two skeletons' heads are 0.67 apart forward and 0.5 up) and weighted
// rigidly to HEAD. OStim's SMP bones aren't kept (no tongue physics yet).
//
//   build_tongues <OStim meshes\OStim\tongues folder> <output folder>
#include "NifFile.hpp"
#include <cstdio>
#include <filesystem>

using namespace nifly;

int main(int argc, char** argv)
{
	const std::filesystem::path in = argv[1], out = argv[2];
	std::filesystem::create_directories(out);
	// HEAD in Fallout 4's (ZeX) skeleton.
	MatTransform head;
	head.translation = Vector3(0.0f, -0.88f, 120.84f);
	const Vector3 shift(0.0f, 0.67f, 0.5f);  // Skyrim's NPC Head (y -1.55, z 120.34) to Fallout 4's HEAD
	int count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(in)) {
		if (entry.path().extension() != ".nif") {
			continue;
		}
		NifFile src;
		if (src.Load(entry.path().string())) {
			printf("can't read %s\n", entry.path().string().c_str());
			continue;
		}
		NifFile nif;
		nif.Create(NiVersion::getFO4());
		nif.GetRootNode()->name.get() = "Scene Root";
		auto* headNode = nif.AddNode("HEAD", head, nif.GetRootNode());
		const int headID = nif.GetBlockID(headNode);
		for (auto* s : src.GetShapes()) {
			std::vector<Vector3> v;
			// Skyrim's verts are in its skin space; its global-to-skin is
			// what Outfit Studio would undo first.
			MatTransform g2s;
			src.CalcShapeTransformGlobalToSkin(s, g2s);
			for (auto& p : *src.GetVertsForShape(s)) {
				v.push_back(g2s.InverseTransform().ApplyTransform(p) + shift);
			}
			std::vector<Triangle> t;
			s->GetTriangles(t);
			auto*                uvp = src.GetUvsForShape(s);
			std::vector<Vector2> uv = uvp ? *uvp : std::vector<Vector2>(v.size());
			auto* shape = nif.CreateShapeFromData(s->name.get(), &v, &t, &uv, nullptr);
			nif.CreateSkinning(shape);
			std::vector<int> ids{ headID };
			nif.SetShapeBoneIDList(shape, ids);
			nif.SetShapeTransformSkinToBone(shape, 0, head.InverseTransform());  // skin space = global
			std::vector<uint8_t> bid{ 0 };
			std::vector<float>   bw{ 1.0f };
			for (size_t i = 0; i < v.size(); ++i) {
				nif.SetShapeVertWeights(shape->name.get(), (uint16_t)i, bid, bw);
			}
			nif.CalcNormalsForShape(shape);
			nif.CalcTangentsForShape(shape);
			nif.UpdateSkinPartitions(shape);
			// Textures under 4Stim's folder.
			for (uint32_t slot = 0; slot < 2; ++slot) {
				std::string tex;
				src.GetTextureSlot(s, tex, slot);
				const auto pos = tex.find("OStim\\tongues\\");
				if (pos != std::string::npos) {
					tex = "textures\\4Stim\\EquipObjects\\tongues\\" + tex.substr(pos + 14);
				}
				nif.SetTextureSlot(shape, tex, slot);
			}
			auto* from = dynamic_cast<BSLightingShaderProperty*>(src.GetShader(s));
			if (auto* ls = dynamic_cast<BSLightingShaderProperty*>(nif.GetShader(shape)); ls && from) {
				ls->glossiness = std::min(from->glossiness / 100.0f, 1.0f);  // Skyrim's glossiness to Fallout 4's smoothness
				ls->specularStrength = std::min(from->specularStrength, 2.0f);
			}
			if (auto* alpha = src.GetAlphaProperty(s)) {
				auto a = std::make_unique<NiAlphaProperty>();
				a->flags = alpha->flags;
				a->threshold = alpha->threshold;
				nif.AssignAlphaProperty(shape, std::move(a));
			}
		}
		nif.Save((out / entry.path().filename()).string());
		++count;
	}
	printf("%d tongue mesh(es)\n", count);
}
