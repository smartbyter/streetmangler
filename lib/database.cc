/*
 * Copyright (C) 2011 Dmitry Marakasov
 *
 * This file is part of streetmangler.
 *
 * streetmangler is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * streetmangler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with streetmangler.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <set>
#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <unicode/unistr.h>
#include <unicode/uchar.h>

#include <tspell/unitrie.hh>

#include <streetmangler/database.hh>
#include <streetmangler/locale.hh>
#include <streetmangler/name.hh>

namespace {
	static const UnicodeString g_yo = UnicodeString::fromUTF8("ё");
	static const UnicodeString g_ye = UnicodeString::fromUTF8("е");

	int PickDist(int olddist, int newdist) {
		return (olddist < 0 || (newdist >= 0 && newdist < olddist)) ? newdist : olddist;
	}
};

namespace StreetMangler {

class Database::Private {
	friend class Database;
protected:
	Private(const Locale& locale) : locale_(locale) {
	}

	const Locale& GetLocale() const {
		return locale_;
	}

	void NameToHashes(const Name& name, std::string* plainhash, UnicodeString* uhash, UnicodeString* uhashordered, UnicodeString* uhashunordered, int extraflags = 0) const {
		static const int flags = Name::STATUS_TO_LEFT | Name::EXPAND_STATUS | Name::NORMALIZE_WHITESPACE | Name::NORMALIZE_PUNCT;
		/* base for a hash - lowercase name with status part at left */
		UnicodeString base_hash = UnicodeString::fromUTF8(name.Join(flags | extraflags)).toLower();

		if (uhash)
			*uhash = base_hash;

		if (plainhash)
			base_hash.toUTF8String(*plainhash);

		if (uhashordered) {
			std::vector<UnicodeString> words;

			int32_t start = 0;
			int32_t end;
			while ((end = base_hash.indexOf(' ', start)) != -1) {
				if (start != end)
					words.push_back(UnicodeString(base_hash, start, end-start));
				start = end + 1;
			}
			if (start != base_hash.length())
				words.push_back(UnicodeString(base_hash, start));

			/* sort all words except for the status part */
			bool sortall = (extraflags & Name::REMOVE_ALL_STATUSES) || !name.HasStatusPart();
			std::sort(sortall ? words.begin() : ++words.begin(), words.end());

			*uhashordered = UnicodeString();
			for (std::vector<UnicodeString>::iterator i = words.begin(); i != words.end(); ++i) {
				if (i != words.begin())
					*uhashordered += " ";
				*uhashordered += *i;
			}
		}

		if (uhashunordered)
			*uhashunordered = UnicodeString::fromUTF8(name.Join((flags & ~Name::STATUS_TO_LEFT) | extraflags)).toLower();
	}

	static int GetRealApproxDistance(const UnicodeString& sample, const UnicodeString& match, int realdepth) {
		/* exact match */
		if (realdepth == 0)
			return 0;

		const UnicodeString& shorter = sample.length() < match.length() ? sample : match;
		const UnicodeString& longer = sample.length() < match.length() ? match : sample;

		int left, right;

		/* get common prefix and suffix */
		for (left = 0; left < shorter.length() && shorter[left] == longer[left]; ++left) {
			/* empty */
		}

		for (right = 0; right < shorter.length() - left && shorter[shorter.length() - 1 - right] == longer[longer.length() - 1 - right]; ++right) {
			/* empty */
		}

		UnicodeString shorterdiff = shorter.tempSubString(left, shorter.length() - left - right);
		UnicodeString longerdiff = longer.tempSubString(left, longer.length() - left - right);

		/* check if there're any non-numeric character in the diff */
		bool shorterdiff_numeric = false;
		for (int i = 0; i < shorterdiff.length(); ++i) {
			if (u_isdigit(shorterdiff.char32At(i))) {
				shorterdiff_numeric = true;
				break;
			}
		}

		bool longerdiff_numeric = false;
		for (int i = 0; i < longerdiff.length(); ++i) {
			if (u_isdigit(longerdiff.char32At(i))) {
				longerdiff_numeric = true;
				break;
			}
		}

		if (shorterdiff_numeric || longerdiff_numeric) {
			bool touches_number = false;
			if (left > 0 && u_isdigit(shorter.charAt(left - 1)))
				touches_number = true;
			if (right > 0 && u_isdigit(shorter.charAt(shorter.length() - right)))
				touches_number = true;

			/* numeric-only match */
			if ((shorterdiff_numeric && longerdiff_numeric) || touches_number)
				return -1;
		}

		/* count swapped adjacent letters as a single typo */
		if (realdepth == 2 && shorterdiff.length() == 2 && longerdiff.length() == 2 && shorterdiff[0] == longerdiff[1] && shorterdiff[1] == longerdiff[0])
			return 1;

		return realdepth;
	}

protected:
    typedef std::set<std::string> NamesSet;
    typedef std::multimap<std::string, std::string> NamesMap;
    typedef std::multimap<UnicodeString, std::string> UnicodeNamesMap;

protected:
    const Locale& locale_;
    NamesSet names_;
    NamesMap canonical_map_;
    UnicodeNamesMap spelling_map_;
    UnicodeNamesMap stripped_map_;

    TSpell::UnicodeTrie spell_trie_;
};

Database::Database(const Locale& locale) : private_(new Database::Private(locale)) {
}

Database::~Database() {
}

void Database::Load(const char* filename) {
	int f;
	if ((f = open(filename, O_RDONLY)) == -1)
		throw std::runtime_error(std::string("Cannot open database: ") + strerror(errno));

	char buffer[1024];
	ssize_t nread;

	std::string name;
	int line = 1;
	bool space = false;
	bool comment = false;
	while ((nread = read(f, buffer, sizeof(buffer))) > 0) {
		for (char* cur = buffer; cur != buffer + nread; ++cur) {
			if (*cur == '\n') {
				if (!name.empty()) {
					Add(name);
					name.clear();
				}
				line++;
				comment = false;
				space = false;
			} else if (comment) {
				/* skip until eol */
			} else if (*cur == '#') {
				comment = true;
			} else if (*cur == ' ' || *cur == '\t') {
				space = true;
			} else {
				if (space && !name.empty()) {
					name += ' ';
					space = false;
				}
				name += *cur;
			}
		}
	}

	if (!name.empty())
		Add(name);

	if (nread == -1)
		throw std::runtime_error(std::string("Read error: ") + strerror(errno));

	close(f);
}

const StreetMangler::Locale& StreetMangler::Database::GetLocale() const {
	return private_->locale_;
}

void Database::Add(const std::string& name) {
	Name tokenized(name, private_->locale_);

	std::string hash;
	UnicodeString uhash;
	UnicodeString uhashordered;
	UnicodeString uhashunordered;

	private_->NameToHashes(tokenized, &hash, &uhash, &uhashordered, &uhashunordered);

	/* for the locales in which canonical form != full form,
	 * we need to use canonical form as a reference */
	std::string canonical = tokenized.Join(Name::CANONICALIZE_STATUS);

	/* for exact match */
	private_->names_.insert(canonical);

	/* for canonical form  */
	private_->canonical_map_.insert(std::make_pair(hash, canonical));

	/* for spelling */
	private_->spell_trie_.Insert(uhash);
	private_->spelling_map_.insert(std::make_pair(uhash, canonical));
	if (uhash != uhashordered) {
		private_->spell_trie_.Insert(uhashordered);
		private_->spelling_map_.insert(std::make_pair(uhashordered, canonical));
	}
	if (uhashunordered != uhash && uhashunordered != uhashordered) {
		private_->spell_trie_.Insert(uhashunordered);
		private_->spelling_map_.insert(std::make_pair(uhashunordered, canonical));
	}

	/* for stripped status  */
	UnicodeString stripped_uhashordered;
	private_->NameToHashes(tokenized, NULL, NULL, &stripped_uhashordered, NULL, Name::REMOVE_ALL_STATUSES);
	stripped_uhashordered.findAndReplace(g_yo, g_ye);
	if (stripped_uhashordered != uhashordered)
		private_->stripped_map_.insert(std::make_pair(stripped_uhashordered, canonical));
}

/*
 * Checks
 */
int Database::CheckExactMatch(const Name& name) const {
	return private_->names_.find(name.Join()) != private_->names_.end();
}

int Database::CheckCanonicalForm(const Name& name, std::vector<std::string>& suggestions) const {
	std::string hash;
	private_->NameToHashes(name, &hash, NULL, NULL, NULL);

	int count = 0;
	std::pair<Private::NamesMap::const_iterator, Private::NamesMap::const_iterator> range =
		private_->canonical_map_.equal_range(hash);

	for (Private::NamesMap::const_iterator i = range.first; i != range.second; ++i, ++count)
		suggestions.push_back(i->second);

	return count;
}

int Database::CheckSpelling(const Name& name, std::vector<std::string>& suggestions, int depth) const {
	UnicodeString hash, hashordered, hashunordered;
	private_->NameToHashes(name, NULL, &hash, &hashordered, &hashunordered);

	int realdepth = 0;
	std::set<UnicodeString> matches;
	for (int i = 0; matches.empty() && i <= depth + 1; ++i) {
		private_->spell_trie_.FindApprox(hash, i, matches);
		private_->spell_trie_.FindApprox(hashordered, i, matches);
		private_->spell_trie_.FindApprox(hashunordered, i, matches);
		realdepth = i;
	}

	std::set<std::string> suggestions_unique;
	for (std::set<UnicodeString>::const_iterator i = matches.begin(); i != matches.end(); ++i) {
		/* skip matches that differ only in numeric parts */
		int dist = -1;
		dist = PickDist(dist, private_->GetRealApproxDistance(hash, *i, realdepth));
		dist = PickDist(dist, private_->GetRealApproxDistance(hashordered, *i, realdepth));
		dist = PickDist(dist, private_->GetRealApproxDistance(hashunordered, *i, realdepth));

		if (dist < 0 || dist > depth)
			continue;

		std::pair<Private::UnicodeNamesMap::const_iterator, Private::UnicodeNamesMap::const_iterator> range =
			private_->spelling_map_.equal_range(*i);

		for (Private::UnicodeNamesMap::const_iterator i = range.first; i != range.second; ++i)
			suggestions_unique.insert(i->second);
	}

	suggestions.reserve(suggestions.size() + suggestions_unique.size());

	for (std::set<std::string>::const_iterator i = suggestions_unique.begin(); i != suggestions_unique.end(); ++i)
		suggestions.push_back(*i);

	return suggestions_unique.size();
}

int Database::CheckStrippedStatus(const Name& name, std::vector<std::string>& matches) const {
	UnicodeString uhashordered;
	private_->NameToHashes(name, NULL, NULL, &uhashordered, NULL);
	uhashordered.findAndReplace(g_yo, g_ye);

	int count = 0;
	std::pair<Private::UnicodeNamesMap::const_iterator, Private::UnicodeNamesMap::const_iterator> range =
		private_->stripped_map_.equal_range(uhashordered);

	for (Private::UnicodeNamesMap::const_iterator i = range.first; i != range.second; ++i, ++count)
		matches.push_back(i->second);

	return count;
}

/*
 * std::string shortcuts to Checks
 */
int Database::CheckExactMatch(const std::string& name) const {
	/* this one a shortcut, actually really */
	return private_->names_.find(name) != private_->names_.end();
}

int Database::CheckCanonicalForm(const std::string& name, std::vector<std::string>& suggestions) const {
	return CheckCanonicalForm(Name(name, private_->locale_), suggestions);
}

int Database::CheckSpelling(const std::string& name, std::vector<std::string>& suggestions, int depth) const {
	return CheckSpelling(Name(name, private_->locale_), suggestions, depth);
}

int Database::CheckStrippedStatus(const std::string& name, std::vector<std::string>& matches) const {
	return CheckStrippedStatus(Name(name, private_->locale_), matches);
}

}
