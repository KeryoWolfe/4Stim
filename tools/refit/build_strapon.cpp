// Refits OStim's Calyps strap-on (Skyrim SE, 3BA) to Fallout 4's True
// Wasteland Body as a BodySlide project: a FO4 nif (harness weighted from
// TWB, dildo weighted to ZeX's Penis_00..05 so 4Stim's penis bend bends it),
// an .osd with TWB's sliders conformed onto it, and an .osp slider set.
#include "NifFile.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

using namespace nifly;

struct Diff { uint16_t i; Vector3 d; };
using OSD = std::map<std::string, std::vector<Diff>>;

OSD ReadOSD(const std::string& path)
{
	OSD out;
	std::ifstream f(path, std::ios::binary);
	uint32_t magic, ver, count;
	f.read((char*)&magic, 4); f.read((char*)&ver, 4); f.read((char*)&count, 4);
	for (uint32_t n = 0; n < count; ++n) {
		uint8_t len; f.read((char*)&len, 1);
		std::string name(len, '\0'); f.read(name.data(), len);
		uint16_t c; f.read((char*)&c, 2);
		auto& v = out[name];
		for (uint16_t k = 0; k < c; ++k) {
			Diff d; f.read((char*)&d.i, 2); f.read((char*)&d.d.x, 4); f.read((char*)&d.d.y, 4); f.read((char*)&d.d.z, 4);
			v.push_back(d);
		}
	}
	return out;
}

void WriteOSD(const std::string& path, const std::vector<std::pair<std::string, std::vector<Diff>>>& sets)
{
	std::ofstream f(path, std::ios::binary);
	uint32_t magic = 0x4F534400, ver = 1, count = (uint32_t)sets.size();
	f.write((char*)&magic, 4); f.write((char*)&ver, 4); f.write((char*)&count, 4);
	for (auto& [name, diffs] : sets) {
		uint8_t len = (uint8_t)name.size(); f.write((char*)&len, 1); f.write(name.data(), len);
		uint16_t c = (uint16_t)diffs.size(); f.write((char*)&c, 2);
		for (auto& d : diffs) { f.write((char*)&d.i, 2); f.write((char*)&d.d.x, 4); f.write((char*)&d.d.y, 4); f.write((char*)&d.d.z, 4); }
	}
}

int main(int argc, char** argv)
{
	const std::string out = argv[1];      // output folder
	const std::string strapPath = argv[2]; // OStim strap-on nif
	const std::string bodyPath = argv[3];
	const std::string skelPath = argv[4];
	const std::string cbbeOSD = argv[5], twbOSD = argv[6], sliderTSV = argv[7];
	const std::string setName = argv[8];   // e.g. "4Stim OStim Strap-On (TWB)"

	NifFile strap, body, skel;
	if (strap.Load(strapPath) || body.Load(bodyPath) || skel.Load(skelPath)) { printf("load failed\n"); return 1; }
	auto* bshape = body.FindBlockByName<NiShape>("True Wasteland Body");
	const auto& bverts = *body.GetVertsForShape(bshape);
	MatTransform g2s; body.CalcShapeTransformGlobalToSkin(bshape, g2s);

	// Body weights per vertex.
	std::vector<std::string> bbones; body.GetShapeBoneList(bshape, bbones);
	std::vector<std::vector<std::pair<int, float>>> bw(bverts.size());
	for (uint32_t b = 0; b < bbones.size(); ++b) {
		std::unordered_map<uint16_t, float> w; body.GetShapeBoneWeights(bshape, b, w);
		for (auto& [v, x] : w) bw[v].push_back({ (int)b, x });
	}

	// Skyrim (3BA) to TWB skin space: fitted to the hips (mean 0.44 units off).
	const float sc = 0.98f, dy = 2.0f, dz = -119.0f;
	auto toTWB = [&](Vector3 p) { return Vector3(p.x * sc, p.y * sc + dy, (p.z - 67.0f) * sc + 67.0f + dz); };

	// ZeX penis chain, in skin space.
	const char* chain[] = { "Penis_00", "Penis_01", "Penis_02", "Penis_03", "Penis_04", "Penis_05" };
	std::vector<MatTransform> chainGlobal(6);
	std::vector<Vector3>      chainSkin(6);
	for (int k = 0; k < 6; ++k) {
		skel.GetNodeTransformToGlobal(chain[k], chainGlobal[k]);
		chainSkin[k] = g2s.ApplyTransform(chainGlobal[k].translation);
		printf("%s skin (%.2f %.2f %.2f)\n", chain[k], chainSkin[k].x, chainSkin[k].y, chainSkin[k].z);
	}

	auto nearest = [&](const Vector3& p) {
		int best = 0; float bd = 1e30f;
		for (size_t i = 0; i < bverts.size(); ++i) {
			float d = (bverts[i] - p).length2();
			if (d < bd) { bd = d; best = (int)i; }
		}
		return best;
	};

	NifFile nif;
	nif.Create(NiVersion::getFO4());
	auto* root = nif.GetRootNode();
	root->name.get() = "Scene Root";

	// Bone nodes, flat under the root, at their bind positions (global).
	std::map<std::string, int> nodeID;
	auto boneNode = [&](const std::string& name) {
		if (auto it = nodeID.find(name); it != nodeID.end()) return it->second;
		MatTransform g;
		if (!skel.GetNodeTransformToGlobal(name, g)) {
			// Not in the skeleton (a body-only helper): from TWB's skin-to-bone.
			MatTransform s2b; body.GetShapeTransformSkinToBone(bshape, name, s2b);
			g = s2b.ComposeTransforms(g2s).InverseTransform();
		}
		auto* node = nif.AddNode(name, g, root);
		int id = nif.GetBlockID(node);
		nodeID[name] = id;
		return id;
	};

	struct Part { std::string name; bool dildo; };
	std::vector<Part> parts = { { "GearCore", false }, { "GearExtra", false }, { "Dildo", true } };
	OSD cbbe = ReadOSD(cbbeOSD), twb = ReadOSD(twbOSD);

	// Sliders: name, invert, default, osd file, key.
	struct SliderData { std::string name, invert, def, file, key; };
	std::vector<SliderData> sliders;
	{
		std::ifstream f(sliderTSV); std::string line;
		while (std::getline(f, line)) {
			std::stringstream ss(line); SliderData s; std::string extra;
			std::getline(ss, s.name, '\t'); std::getline(ss, s.invert, '\t'); std::getline(ss, s.def, '\t');
			std::getline(ss, s.file, '\t'); std::getline(ss, s.key, '\t');
			if (!s.name.empty()) sliders.push_back(s);
		}
	}
	// Body diff per slider as a full array.
	auto bodyDiff = [&](const SliderData& s) {
		std::vector<Vector3> d(bverts.size());
		auto& src = s.file == "CBBEBody.osd" ? cbbe : twb;
		auto it = src.find(s.key);
		if (it != src.end()) for (auto& x : it->second) if (x.i < d.size()) d[x.i] = x.d;
		return d;
	};
	std::vector<std::vector<Vector3>> bodyDiffs;
	for (auto& s : sliders) bodyDiffs.push_back(bodyDiff(s));

	std::vector<std::pair<std::string, std::vector<Diff>>> osdOut;
	std::vector<std::string> shapeNames;

	for (auto& part : parts) {
		auto* src = strap.FindBlockByName<NiShape>(part.name);
		if (!src) { printf("no shape %s\n", part.name.c_str()); continue; }
		std::vector<Vector3> v; for (auto& p : *strap.GetVertsForShape(src)) v.push_back(toTWB(p));
		std::vector<Triangle> t; src->GetTriangles(t);
		auto* uvp = strap.GetUvsForShape(src);
		std::vector<Vector2> uv = uvp ? *uvp : std::vector<Vector2>(v.size());
		const std::string name = "OStim Strap-On " + part.name;
		auto* shape = nif.CreateShapeFromData(name, &v, &t, &uv, nullptr);
		nif.CreateSkinning(shape);
		shapeNames.push_back(name);

		// Per-vertex weights (bone name -> weight).
		std::vector<std::vector<std::pair<std::string, float>>> w(v.size());
		std::vector<int> nearestBody(v.size());
		int anchor = -1;
		if (part.dildo) {
			// Rigidly follow the body where the dildo's base meets it.
			Vector3 base = chainSkin[0];
			anchor = nearest(base);
		}
		for (size_t i = 0; i < v.size(); ++i) {
			if (!part.dildo) {
				int n = nearest(v[i]);
				nearestBody[i] = n;
				for (auto& [b, x] : bw[n]) w[i].push_back({ bbones[b], x });
			} else {
				nearestBody[i] = anchor;
				float y = v[i].y;
				if (y <= chainSkin[0].y) { w[i].push_back({ chain[0], 1.0f }); continue; }
				if (y >= chainSkin[5].y) { w[i].push_back({ chain[5], 1.0f }); continue; }
				for (int k = 0; k < 5; ++k) {
					if (y <= chainSkin[k + 1].y) {
						float f = (y - chainSkin[k].y) / (chainSkin[k + 1].y - chainSkin[k].y);
						if (1.0f - f > 0.001f) w[i].push_back({ chain[k], 1.0f - f });
						if (f > 0.001f) w[i].push_back({ chain[k + 1], f });
						break;
					}
				}
			}
		}
		// Bone list for this shape.
		std::vector<std::string> bones;
		for (auto& vw : w) for (auto& [b, x] : vw) if (std::find(bones.begin(), bones.end(), b) == bones.end()) bones.push_back(b);
		std::vector<int> ids; for (auto& b : bones) ids.push_back(boneNode(b));
		nif.SetShapeBoneIDList(shape, ids);
		for (uint32_t bi = 0; bi < bones.size(); ++bi) {
			MatTransform g; nif.GetNodeTransformToGlobal(bones[bi], g);
			nif.SetShapeTransformSkinToBone(shape, bi, g.InverseTransform().ComposeTransforms(g2s.InverseTransform()));
		}
		for (size_t i = 0; i < v.size(); ++i) {
			// Up to 4, normalized.
			auto& vw = w[i];
			std::sort(vw.begin(), vw.end(), [](auto& a, auto& b) { return a.second > b.second; });
			if (vw.size() > 4) vw.resize(4);
			float sum = 0; for (auto& x : vw) sum += x.second;
			std::vector<uint8_t> bid; std::vector<float> bwt;
			for (auto& [b, x] : vw) { bid.push_back((uint8_t)(std::find(bones.begin(), bones.end(), b) - bones.begin())); bwt.push_back(sum > 0 ? x / sum : 0); }
			nif.SetShapeVertWeights(name, (uint16_t)i, bid, bwt);
		}
		nif.CalcNormalsForShape(shape);
		nif.CalcTangentsForShape(shape);
		nif.UpdateSkinPartitions(shape);

		// Textures: OStim's, under a 4Stim path.
		std::string diffuse = "textures\\4Stim\\EquipObjects\\calypsStrapon.dds", normal = "textures\\4Stim\\EquipObjects\\calypsStrapon_n.dds";
		nif.SetTextureSlot(shape, diffuse, 0);
		nif.SetTextureSlot(shape, normal, 1);
		if (auto* ls = dynamic_cast<BSLightingShaderProperty*>(nif.GetShader(shape))) {
			ls->glossiness = 0.35f;        // Fallout 4's smoothness (no _s map: kept low)
			ls->specularStrength = 0.4f;
		}

		// Conformed sliders: each vertex moves as the body vertex nearest it.
		for (size_t s = 0; s < sliders.size(); ++s) {
			std::vector<Diff> diffs;
			for (size_t i = 0; i < v.size(); ++i) {
				const auto& d = bodyDiffs[s][nearestBody[i]];
				if (d.length2() > 1e-10f) diffs.push_back({ (uint16_t)i, d });
			}
			if (!diffs.empty()) osdOut.push_back({ name + sliders[s].name, diffs });
		}
		printf("%s: %zu verts, %zu tris, %zu bones\n", name.c_str(), v.size(), t.size(), bones.size());
	}

	nif.Save(out + "/" + setName + ".nif");
	WriteOSD(out + "/" + setName + ".osd", osdOut);

	// The slider set.
	std::ofstream osp(out + "/" + setName + ".osp");
	osp << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SliderSetInfo version=\"1\">\n";
	osp << "    <SliderSet name=\"" << setName << "\">\n";
	osp << "        <DataFolder>" << setName << "</DataFolder>\n";
	osp << "        <SourceFile>" << setName << ".nif</SourceFile>\n";
	osp << "        <OutputPath>meshes\\4Stim\\EquipObjects</OutputPath>\n";
	osp << "        <OutputFile GenWeights=\"false\">OStimStrapOn</OutputFile>\n";
	for (auto& s : shapeNames) osp << "        <Shape target=\"" << s << "\">" << s << "</Shape>\n";
	for (auto& sl : sliders) {
		osp << "        <Slider name=\"" << sl.name << "\" invert=\"" << sl.invert << "\" default=\"" << sl.def << "\">\n";
		for (auto& s : shapeNames) {
			bool has = std::any_of(osdOut.begin(), osdOut.end(), [&](auto& e) { return e.first == s + sl.name; });
			if (has) osp << "            <Data name=\"" << s << sl.name << "\" target=\"" << s << "\" local=\"true\">" << setName << ".osd\\" << s << sl.name << "</Data>\n";
		}
		osp << "        </Slider>\n";
	}
	osp << "    </SliderSet>\n</SliderSetInfo>\n";
	printf("wrote %s (%zu slider data sets)\n", setName.c_str(), osdOut.size());
}
