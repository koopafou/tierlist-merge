#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <map>
#include <vector>
#include <algorithm>

struct ScoringConf {
	std::string name;
	double baseparts;
};

struct Score {
	const ScoringConf* conf;
	std::vector<double> pickscores;
	std::vector<int> ranking;

	Score (int sz)
		: conf(nullptr)
		, pickscores(sz, 0.0)
		, ranking(sz) {
		for (int i = 0; i < sz; ++i)
			ranking[i] = i;
	}

	void add_tl (const std::vector<std::vector<int>>& tiers) {
		double parts = 0;
		for (int tier = 0; tier < tiers.size(); ++tier)
			parts += (conf->baseparts + tiers.size() - tier - 1) * tiers[tier].size();
		double div = 1.0 / parts;
		for (int tier = 0; tier < tiers.size(); ++tier)
			for (int pick = 0; pick < tiers[tier].size(); ++pick)
				pickscores[tiers[tier][pick]] += div * (conf->baseparts + tiers.size() - tier - 1);
	}

	void rank () {
		std::sort(ranking.begin(), ranking.end(), [&] (int i, int j) {
			return (pickscores[i] > pickscores[j]);
		});
	}
};

#define SHIFT() ++arg; if (arg == argc) return 1

int main (int argc, char** argv) {
	bool diffrank = false;
	bool graph = false;
	int gr_height = 0;
	int gr_picbox = 0;
	std::string graph_pics;
	for (int arg = 1; arg < argc; ++arg) {
			if (std::string("--diff") == argv[arg])
				diffrank = true;
			if (std::string("--graph") == argv[arg]) {
				graph = true;
				SHIFT();
				graph_pics = argv[arg];
				SHIFT();
				gr_height = std::atoi(argv[arg]);
				SHIFT();
				gr_picbox = std::atoi(argv[arg]);
			}
	}

	constexpr int scorescale = 100000;

	std::vector<ScoringConf> confs;
	while (std::cin.peek() != '+') {
		ScoringConf conf;
		std::getline(std::cin, conf.name, ' ');
		std::cin >> conf.baseparts;
		std::cin.ignore();
		confs.push_back(conf);
	}
	std::cin.ignore();
	int picksz = 0;
	std::map<std::string, int> pickmap;
	std::string pickline;
	std::getline(std::cin, pickline);
	std::stringstream picksstr;
	picksstr << pickline;
	while (true) {
		std::string pick;
		std::getline(picksstr, pick, ' ');
		if (!picksstr)
			break;
		pickmap[pick] = picksz;
		++picksz;
	}

	int gr_width = gr_picbox * picksz;
	std::ofstream sgraph;
	if (graph) {
		sgraph.open("magick-ranking", std::ios_base::out);
		sgraph << "-size " << gr_width << "x" << gr_height << " xc:none ";
	}

	std::vector<Score> scores(confs.size(), picksz);
	for (int cf = 0; cf < confs.size(); ++cf)
		scores[cf].conf = &confs[cf];
	std::vector<std::string> picknames(picksz);
	for (const auto& [name, index] : pickmap)
		picknames[index] = name;
	while (std::cin) {
		std::string nickname;
		std::getline(std::cin, nickname);
		// strip "-- "
		std::vector<std::vector<int>> tiers;
		std::vector<bool> confirmed(picksz, false);
		while (std::cin && std::cin.peek() != '-') {
			std::string tiername;
			std::getline(std::cin, tiername, ':');
			tiers.push_back({});
			std::string tierpicks;
			std::getline(std::cin, tierpicks);
			std::stringstream tpsstr;
			tpsstr << tierpicks;
			while (true) {
				std::string tp;
				std::getline(tpsstr, tp, ' ');
				if (!tpsstr)
					break;
				if (tp == "")
					continue;
				auto it = pickmap.find(tp);
				if (it == pickmap.end()) {
					std::cerr << "Unknown pick " << nickname << " / " << tiername << " / " << tp << std::endl;
					continue;
				}
				if (confirmed[it->second]) {
					std::cerr << "Double pick " << nickname << " / " << tiername << " / " << tp << std::endl;
					continue;
				}
				tiers.back().push_back(it->second);
				confirmed[it->second] = true;
			}
		}
		bool abort_tl = false;
		for (int i = 0; i < picksz; ++i)
			if (!confirmed[i]) {
				std::cerr << "Missing pick " << nickname << " / " << picknames[i] << std::endl;
				abort_tl = true;
			}
		if (abort_tl)
			continue;
		for (int cf = 0; cf < scores.size(); ++cf)
			scores[cf].add_tl(tiers);
	}
	for (int cf = 0; cf < scores.size(); ++cf)
		scores[cf].rank();
	int maxlen = 0;
	for (int i = 0; i < picksz; ++i)
		if (maxlen < picknames[i].length())
			maxlen = picknames[i].length();
	std::cout << std::fixed << std::setprecision(0);
	/* output lines in the column :
	   >  Confname1                   Confname2
	   >Pickname-foobar :     XXX   Pickname-awoo   :  ZZZZZZ
	   >Pickname-quux   :      YY   Pickname-foobar :    XXXX
	   ...
	   column_width = max(2 + conf->name.length() + 3,
	                      max_of{pickname.length()} + 3 + {score_width} + 3);
	*/
	auto space = [] (int this_length, int ceil_length) {
		while (this_length < ceil_length) {
			std::cout << ' ';
			++this_length;
		}
	};
	auto spc_confname = [&] (int cf) {
		return 2 + scores[cf].conf->name.length() + 3;
	};
	static constexpr int score_width = 6;
	int spc_pickname = maxlen + 3 + score_width + 3;
	for (int cf = 0; cf < scores.size(); ++cf) {
		std::cout << "  " << scores[cf].conf->name << "   ";
		space(spc_confname(cf), spc_pickname);
	}
	std::cout << '\n';
	double prevscore = 0.0;
	for (int j = 0; j < picksz; ++j) {
		for (int cf = 0; cf < scores.size(); ++cf) {
			double scoremax = scores[cf].pickscores[scores[cf].ranking[0]];
			double scoremin = scores[cf].pickscores[scores[cf].ranking.back()];
			int i = scores[cf].ranking[j];
			double normscore = (scores[cf].pickscores[i] - scoremin) / (scoremax - scoremin);
			double score = scorescale * normscore;
			std::cout << picknames[i];
			space(picknames[i].length(), maxlen);
			std::cout << " : " << std::setw(score_width) << std::right;
			if (!diffrank)
				std::cout << score << "   ";
			else if (j == 0)
				std::cout << "   ";
			else
				std::cout << (prevscore - score) << "   ";
			prevscore = score;
			space(spc_pickname, spc_confname(cf));
			if (graph) {
				sgraph << "( -resize " << gr_picbox << "x" << gr_picbox << " "
				       << graph_pics << "/" << picknames[i] << ".png )"
				       << " -geometry "
				       << "+" << (int)((1 - (double)j / (picksz-1)) * (gr_width - gr_picbox))
				       << "+" << (int)((1 - normscore) * (gr_height - gr_picbox))
				       << " -composite ";
			}
		}
		std::cout << '\n';
	}
	std::cout << std::flush;
	if (graph)
		sgraph << " ranks.png" << std::endl;
	return 0;
}
