#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

#include "core/assert.h"

using json = nlohmann::json;

std::mt19937 rng(std::random_device{}());

// ---- DATA STRUCTURES ----
struct Planet {
	std::vector<struct Trait> planet_traits;
	int index;
};

enum class TraitQuality {
	BAD,
	GOOD,
	RANDOM
};

struct Trait {
	std::string description;
	TraitQuality quality;
	std::string category;
	bool used = false;
};

// ---- HELPERS ----
template <typename T>
T random_choice(const std::vector<T>& vec) {
	std::uniform_int_distribution<> dist(0, vec.size() - 1);
	return vec[dist(rng)];
}

template <typename T>
std::vector<T> random_sample(const std::vector<T>& vec, int count) {
	std::vector<T> copy = vec;
	std::shuffle(copy.begin(), copy.end(), rng);
	copy.resize(count);
	return copy;
}

// ---- ROUND SETUP ----
std::tuple<json, std::vector<std::string>, std::vector<std::string>> generate_creature(
	int human_trait_count, const json& data, const std::vector<std::string>& categories
) {
	auto chosen_categories = random_sample(categories, human_trait_count);
	json creature;

	for (const auto& cat : chosen_categories) {
		std::vector<std::string> traits;
		for (auto& [key, _] : data["creatures"][cat].items()) {
			traits.push_back(key);
		}

		creature[cat] = random_choice(traits);
	}

	std::vector<std::string> leftover_categories;
	for (const auto& c : categories) {
		if (std::find(chosen_categories.begin(), chosen_categories.end(), c) ==
			chosen_categories.end()) {
			leftover_categories.push_back(c);
		}
	}

	return { creature, chosen_categories, leftover_categories };
}

std::tuple<std::vector<Trait>, std::vector<Trait>, std::vector<Trait>> generate_trait_list(
	const json& data, const json& creature, const std::vector<std::string>& chosen_categories,
	const std::vector<std::string>& leftover_categories
) {
	std::vector<Trait> good_traits, bad_traits, random_traits;

	for (const auto& cat : chosen_categories) {
		std::string human_trait = creature[cat];

		for (const auto& pt : data["creatures"][cat][human_trait]["bad"]) {
			bad_traits.push_back({ pt, TraitQuality::BAD, cat });
		}

		for (const auto& pt : data["creatures"][cat][human_trait]["good"]) {
			good_traits.push_back({ pt, TraitQuality::GOOD, cat });
		}
	}

	for (const auto& cat : leftover_categories) {
		for (auto& [ht, value] : data["creatures"][cat].items()) {
			std::vector<std::string> combined;

			for (const auto& b : value["bad"]) {
				combined.push_back(b);
			}
			for (const auto& g : value["good"]) {
				combined.push_back(g);
			}

			assert(!combined.empty());

			for (const auto& pt : combined) {
				random_traits.push_back({ pt, TraitQuality::RANDOM, cat });
			}
		}
	}

	return { good_traits, bad_traits, random_traits };
}

// ---- PLANET GENERATION ----
Planet generate_planet(
	int planet_trait_count, std::vector<Trait> good_traits, std::vector<Trait> bad_traits,
	std::vector<Trait> random_traits, int leftover_category_count, std::pair<int, int> count,
	int index
) {
	auto [good_count, bad_count] = count;
	std::vector<Trait> planet_traits;

	// GOOD
	for (int i = 0; i < good_count; ++i) {
		if (good_traits.empty()) {
			break;
		}
		auto choice = random_choice(good_traits);
		planet_traits.push_back(choice);

		good_traits.erase(
			std::remove_if(
				good_traits.begin(), good_traits.end(),
				[&](const Trait& t) { return t.category == choice.category; }
			),
			good_traits.end()
		);

		bad_traits.erase(
			std::remove_if(
				bad_traits.begin(), bad_traits.end(),
				[&](const Trait& t) { return t.category == choice.category; }
			),
			bad_traits.end()
		);
	}

	int needed_traits = planet_trait_count - (good_count + bad_count);

	PTGN_ASSERT(needed_traits >= 0, "More planet traits demanded than are available");

	// BAD
	for (int i = 0; i < bad_count; ++i) {
		if (bad_traits.empty()) {
			needed_traits += bad_count - i;
			break;
		}

		auto choice = random_choice(bad_traits);
		planet_traits.push_back(choice);

		bad_traits.erase(
			std::remove_if(
				bad_traits.begin(), bad_traits.end(),
				[&](const Trait& t) { return t.category == choice.category; }
			),
			bad_traits.end()
		);
	}

	if (leftover_category_count < needed_traits) {
		std::cout << "Warning: not enough random traits!\n";
	}

	// RANDOM
	for (int i = 0; i < needed_traits; ++i) {
		if (random_traits.empty()) {
			break;
		}

		auto choice = random_choice(random_traits);
		planet_traits.push_back(choice);

		random_traits.erase(
			std::remove_if(
				random_traits.begin(), random_traits.end(),
				[&](const Trait& t) { return t.category == choice.category; }
			),
			random_traits.end()
		);
	}

	return { planet_traits, index };
}

// ---- GAME ROUND ----
void play_round(
	int human_trait_count, int planet_trait_count, const json& data,
	const std::vector<std::string>& categories, const std::vector<std::pair<int, int>>& patterns
) {
	auto [creature, chosen_categories, leftover_categories] =
		generate_creature(human_trait_count, data, categories);

	auto [good_traits, bad_traits, random_traits] =
		generate_trait_list(data, creature, chosen_categories, leftover_categories);

	std::vector<Planet> planets;

	for (int i = 0; i < patterns.size(); ++i) {
		PTGN_ASSERT(
			planet_trait_count >= patterns[i].first + patterns[i].second,
			"More planet traits demanded than are available"
		);

		planets.push_back(generate_planet(
			planet_trait_count, good_traits, bad_traits, random_traits, leftover_categories.size(),
			patterns[i], i
		));
	}

	std::shuffle(planets.begin(), planets.end(), rng);

	int winner = 0;
	for (int i = 0; i < planets.size(); ++i) {
		if (planets[i].index == 0) {
			winner = i;
		}
	}

	// OUTPUT
	std::cout << "\nCREATURE TRAITS:\n";
	for (const auto& cat : chosen_categories) {
		std::cout << "- " << creature[cat] << "\n";
	}

	std::cout << "\nPLANETS:\n";
	for (int i = 0; i < planets.size(); ++i) {
		std::cout << "\nPlanet " << i + 1 << ":\n";
		for (const auto& trait : planets[i].planet_traits) {
			std::cout << "  " << trait.description << "\n";
		}
	}

	int choice;
	std::cout << "\nWhich planet is best? (1-3): ";
	std::cin >> choice;
	choice -= 1;

	if (choice == winner) {
		std::cout << "\nCorrect!\n";
	} else {
		std::cout << "\nIncorrect.\n";
	}

	std::cout << "\nBest planet was: Planet " << winner + 1 << "\n";

	std::cout << "\nDETAILS:\n";
	for (int i = 0; i < planets.size(); ++i) {
		std::cout << "\nPlanet " << i + 1 << ":\n";
		for (const auto& trait : planets[i].planet_traits) {
			switch (trait.quality) {
				case TraitQuality::GOOD:   std::cout << " + " << trait.description << "\n"; break;
				case TraitQuality::BAD:	   std::cout << " - " << trait.description << "\n"; break;
				case TraitQuality::RANDOM: std::cout << " ? " << trait.description << "\n"; break;
			}
		}
	}
}

// ---- MAIN ----
int main() {
	std::ifstream f("assets/traits.json");
	if (!f) {
		std::cerr << "Failed to open json\n";
		return 1;
	}

	json data;
	f >> data;

	std::vector<std::string> categories;
	for (auto& [key, _] : data["creatures"].items()) {
		categories.push_back(key);
	}

	play_round(3, 3, data, categories, { { 3, 0 }, { 2, 1 }, { 1, 2 } });

	return 0;
}
