/*  Stream base/radix conversion
    This takes a data stream and converts the radix from x to y. Radices can be anything from 2 up to about 4294967296...
    Copyright (C) 2017  Justin Swatsenbarg

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <streambuf>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cctype>

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x < y ? y : x)
#define SIZE_CHANGE(L, F, T) (L * ceil(log2(MAX(F, T)) / log2(MIN(F, T))))

#define WC2UI16(c) ((*(uint16_t *)&c))
#if('ab' == (('a' << 8) | 'b'))
    #define ENDIAN(wb) ((uint16_t)((wb << 8) | (wb & wb & 0xFF00) >> 8))
#else
    #define ENDIAN(wb) ((uint16_t)(wb))
#endif

using namespace std;

/* Description: Converts between radices (bases) between 2 and 4294967297 as a stream */

class base : public basic_streambuf<char>
{
public:
	enum action { IGNORE, ZERO, QUIT, INFORM, EXIT, THROW };
	enum flags { INLOWER = 0, INLOW = 0, INUPPER = 1, INUP = 1, INKEEP = 2, INNUMERIC = 4,
	             OUTLOWER = 0, OUTLOW = 0, OUTUPPER = 8, OUTUP = 8, OUTKEEP = 16, OUTNUMERIC = 32 };
private:
	bool in_case, out_case, in_keep_case, out_keep_case;
	enum action action_invalid;
	uint16_t _chunk_size, _reads, _writes;
	uint32_t _from, _to;
	vector<uint32_t> buffer;
	vector<char32_t> index2char, char2index;
	uint32_t forcecase(uint32_t c) { return out_keep_case ? c : out_case ? toupper(c) : tolower(c); }
	char32_t standardcase(unsigned char c) { return (char32_t)(in_keep_case ? c : in_case ? toupper(c) : tolower(c)); }
	string bl;
	void convert(vector<uint32_t>, vector<uint32_t> &);
	void negotiatebase(uint32_t, uint32_t);
public:
	base(uint32_t, uint32_t, const string &, const string &, flags, action);
	/* Description: Sets map Char2Index from c2i string
	 * Parameters:
	 *	c2i: String of characters to use for the input character to number conversion
	 */
	void setchar2index(string c2i) { char2index.clear(); for (uint32_t i = 0; i < c2i.size(); i++) char2index[standardcase(c2i[standardcase(i)])] = (uint32_t)i; }
	/* Description: Sets map Index2Char from i2c string
	 * Parameters:
	 *	i2c: String of characters to use for the output character to number conversion
	 */
	void setindex2char(string i2c) { index2char.clear(); for (uint32_t i = 0; i < i2c.size(); i++) index2char[i] = standardcase(i2c[standardcase(i)]); }
	void seti2cslice(uint16_t s, uint16_t e) { setchar2index(bl.substr(s, e)); }
	void setc2islice(uint16_t s, uint16_t e) { setindex2char(bl.substr(s, e)); }

	base &operator <<(uint64_t);
	base &operator <<(vector<uint8_t>);
	base &operator <<(vector<uint16_t>);
	base &operator <<(vector<uint32_t>);
	base &operator <<(vector<uint64_t>);
	base &operator <<(string);
	base &operator = (vector<uint8_t> &);
	base &operator = (vector<uint16_t> &);
	base &operator = (vector<uint32_t> &);
	base &operator = (vector<uint64_t> &);
	base &operator = (string);
	base &operator >>(uint64_t &);
	base &operator >>(vector<uint8_t> &);
	base &operator >>(vector<uint16_t> &);
	base &operator >>(vector<uint32_t> &);
	base &operator >>(vector<uint64_t> &);
	base &operator >>(string &);
	operator vector<uint8_t> ();
	operator vector<uint16_t> ();
	operator vector<uint32_t> ();
	operator vector<uint64_t> ();
	operator string();

	uint32_t frombase(uint32_t f = 0) { if (f > 1) negotiatebase(_from = f, _to); return _from; }
	uint32_t tobase(uint32_t t = 0) { if (t > 1) negotiatebase(_from, _to = t); return _to; }
	uint32_t readsize() { return _reads; } ///////uint32_t z = ceil(log2(f) / 8);
	uint32_t writesize() { return _writes; }

	void inlowercase() { in_case = false; }
	void inuppercase() { in_case = true; }
	char zero() { return index2char[0]; } // For fill cases where zero is not the character '0'
	void setinvalid(action a) { action_invalid = a; }
	void setuppercase() { out_case = true; in_case = false; }
	void setlowercase() { out_case = in_case = false; }

	friend ostream &operator <<(ostream &, base &);
	friend istream &operator >>(istream &, base &);
};

/* Parameters:
 *	f: From radix
 *	t: To radix
 *	fs: From string; must be a string of unique characters
 *	ts: To string; must be a string of unique characters
 *	inUpperCase: Force input case to uppercase (otherwise lowercase)
 *	outUpperCase: Force output case to uppercase (otherwise lowercase)
 *	todo: Action to take when an invalid character is encountered
 */
base::base(uint32_t f = 36, uint32_t t = 36, const string &fs = "", const string &ts = "", flags flag = (flags)(INUPPER | OUTUPPER), action invact = IGNORE)
{
	bl = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	string _fs(bl), _ts(bl);
	setinvalid(invact);
	if(!(in_keep_case = flag & INKEEP)) in_case = flag & INUPPER;
	if(!(out_keep_case = flag & OUTKEEP)) out_case = flag & OUTUPPER;
	if(!in_case && f > 36) in_keep_case = true;
	if(!out_case && t > 36) out_keep_case = true;
	if(f < 2 || t < 2) throw "Invalid radix";
	char2index.resize(f);
	index2char.resize(t);
	if(flag & INNUMERIC) for (uint64_t i = 0; i < f && i < f; i++) char2index[i] = i;
	else
	{
		if(fs.size())
		{
			if(fs.size() < f) cerr << "From radix string is too small--default used!" << endl;
			_fs.assign(fs);
		}
		else _fs.assign(bl.substr(0, f));
		for (uint32_t i = 0; i < _fs.size() && i < f; i++) char2index[standardcase(_fs[i])] = i;
	}
	if(flag & OUTNUMERIC) for (uint64_t i = 0; i < t && i < t; i++) index2char[i] = i;
	else
	{
		if(ts.size())
		{
			if(ts.size() < t) cerr << "To radix string is too small--default used!" << endl;
			_ts.assign(ts);
		}
		else _ts.assign(bl.substr(0, t));
		for (uint32_t i = 0; i < _ts.size() && i < t; i++) index2char[i] = standardcase(_ts[i]);
	}
	negotiatebase(f, t);
}

/* Description: Sets related variables based on input, output radix ratios
 * Parameters:
 *	f: From radix
 * 	t: To radix
 */
void base::negotiatebase(uint32_t f, uint32_t t)
{
	uint32_t bpd = ceil(log2(f) / 8);
	_from = f;
	_to = t;
	_chunk_size = (uint16_t)SIZE_CHANGE(1, f, t); // We take max, min to ensure the ratio is not fractional
	if (_from < _to) { _reads = _chunk_size; _writes = bpd; }
	else { _writes = _chunk_size; _reads = bpd; }
	if(buffer.size() < (1 + (buffer.size() / _chunk_size))) buffer.resize(1 + (buffer.size() / _chunk_size)); // Round size up to nearest multiple of _chunk_size
}

#define OPLSFN vector<uint32_t> in(v.begin(), v.end()); convert(in, buffer); return *this
base &base::operator <<(uint64_t u) { vector<uint32_t> x(1); x[0] = u; buffer.resize(SIZE_CHANGE(x.size(), _from, _to)); convert(x, buffer); return *this; }
base &base::operator <<(vector<uint8_t> v) { OPLSFN; }
base &base::operator <<(vector<uint16_t> v) { OPLSFN; }
base &base::operator <<(vector<uint32_t> v) { convert(v, buffer); return *this; }
base &base::operator <<(vector<uint64_t> v) { OPLSFN; }
base &base::operator <<(string s)
{
	vector<uint32_t> in(s.size());
	for (uint32_t i = 0, o = 0; i < s.size(); i++)
	{
		try { in[i - o] = char2index.at(standardcase(s[i])); }
		catch(out_of_range)
		{
			switch(action_invalid)
			{
				case ZERO: in[i] = 0; break;
				case QUIT: return *this;
				case EXIT: exit(1);
				case THROW: throw out_of_range("Invalid index!");
				case INFORM: cerr << "Invalid value: '" << s[i] << "'" << endl;
				case IGNORE:
					o++; // Now the input does not track perfectly w/ the output--apply an offset
					in.resize(in.size() - 1); // Free up the extra digit
				default: break;
			}
		}
	}
	convert(in, buffer);
	return *this;
}

#define OPEQFN(var) vector<uint32_t> in((var).begin(), (var).end()); buffer.clear(); convert(in, buffer); return *this
base &base::operator = (vector<uint8_t> &v)  { OPEQFN(v); }
base &base::operator = (vector<uint16_t> &v) { OPEQFN(v); }
base &base::operator = (vector<uint32_t> &v) { OPEQFN(v); }
base &base::operator = (vector<uint64_t> &v) { OPEQFN(v); }
base &base::operator = (const string s)
{
    vector<uint32_t> in(s.size());
    for (uint32_t i = 0; i < in.size(); i++) in[i] = char2index[forcecase(s[i])];
    buffer.clear();
    convert(in, buffer);
    return *this;
}
#define OPRSFN v.assign(buffer.begin(), buffer.end()); return *this
base &base::operator >>(uint64_t &u) { u = buffer.front(); buffer.erase(buffer.begin(), buffer.begin() + 1);  return *this; }
base &base::operator >>(vector<uint8_t> &v)  { OPRSFN; }
base &base::operator >>(vector<uint16_t> &v) { OPRSFN; }
base &base::operator >>(vector<uint32_t> &v) { OPRSFN; }
base &base::operator >>(vector<uint64_t> &v) { OPRSFN; }
base &base::operator >>(string &s)
{
	s.clear();
	s.resize(buffer.size());
	for (uint32_t i = 0; i < buffer.size(); i++)
	{
	    uint32_t x = buffer[i],
	    y = index2char[x],
	    z = forcecase(y);
	    s[i] = z;
	}
	return *this;
}
base::operator vector<uint8_t>()  { vector<uint8_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint16_t>() { vector<uint16_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint32_t>() { vector<uint32_t> b(buffer.begin(), buffer.end()); return b; }
base::operator vector<uint64_t>() { vector<uint64_t> b(buffer.begin(), buffer.end()); return b; }
base::operator string() { string s(buffer.begin(), buffer.end()); return s; } // Raw index string

ostream& operator <<(ostream &os, base &val)
{
	for (vector<uint32_t>::const_iterator i = val.buffer.begin(); i < val.buffer.end(); ++i) os << (char)val.index2char[*i];
	os << endl;
	return os;
}
istream& operator >>(istream &is, base &se)
{
	vector<uint32_t> in;
	string tmp;
	is >> tmp;
	in.reserve(tmp.size());
	for (string::const_iterator i = tmp.begin(); i < tmp.end(); ++i) in.push_back(se.char2index.at(*i));
	se.convert(in, se.buffer);
	return is;
}

/* Description: Converts number in to number out radix on from and to radix
 * Parameters:
 *	in: Vector where each array element is one digit in from radix
 *	out: Vector to place output where each array element is one digit in to radix
 */
void base::convert(vector<uint32_t> in, vector<uint32_t> &out)
{
	int64_t a, i, t, x, z;
	out.clear();
	out.resize((uint32_t)SIZE_CHANGE(in.size(), _from, _to));
	while (in.size() > 1 && in[0] == 0) in.erase(in.begin(), in.begin() + 1); // Remove leading zeros; Alt: in.erase(in.begin(), in.begin() + min(in.find_first_not_of('0'), in.size() - 1));
	for (i = in.size() - 1, x = 1; i >= 0; i--, x *= _from)
		for (a = x * in[i], t = out.size() - 1; a > 0; a += out[t], out[t > 0 ? t-- : 0] = a % _to, a /= _to);
	while (out.size() > 1 && out[0] == 0) out.erase(out.begin(), out.begin() + 1); // Remove leading zeros; Alt: out.erase(out.begin(), out.begin() + min(out.find_first_not_of('\0'), out.size() - 1));
}

int printHelp()
{
	cout <<
		"Stream Arbitrary Base Converter    Copyright (C) 2017  Justin Swatsenbarg\n"
		"This takes a data stream and converts the radix from x to y.\n"
		"Radices can be anything from 2 up to about 4294967296...\n\n"
		"This program comes with ABSOLUTELY NO WARRANTY!\n"
		"This is free software, and you are welcome to redistribute\n"
		"it under certain conditions of the GPL v2 license.\n"
		"See <http://www.gnu.org/licenses/> for details.\n"
		"\n"
		"Prefixing each option with '-' is optional\n"
		"From:                           To:\n"
		" fr[adix]                        tr[adix]\n"
		" fs[tring]=X                     ts[tring]=X\n"
		"    Default string is \"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\"\n"
		"Numeric (no string):\n"
		" fn[ostring]                     tn[ostring]\n"
		"Case:                     Note: From case is only useful with input radix string\n"
		" fl[owercase]                    tl[owercase]\n"
		" fu[ppercase]                    tu[ppercase]\n"
		"Shortcut\n"
		"  Equivalent to fr=v or tr=v respectively where v is the radix value\n"
		" f2, fb[inary]                   t2, tb[inary]\n"
		" f8, fo[ctal]                    t8, to[ctal]\n"
		" f1[0], fd[ecimal]               t1[0], td[ecimal]\n"
		" fx, fh[exidecimal]              tx, th[exidecimal]\n"
		" ft, f3[6], fH[exatrigesimal]    tt, t3[6], tH[exatrigesimal]\n"
		"Combo:\n"
		" fa[scii]                        ta[scii] Sets radix 256 and the respective string to ascii\n"
		"Invalid character action\n"
		" ig[nore], sk[ip]       Ignore (skip) and continue\n"
		" ze[ro]                 Zero invalid values\n"
		" qu[uit], st[op]        Quit reading input\n"
		" in[form], dr[op]       Inform on stderr, ignore and continue\n"
		" ex[it], er[ror]        Exit program with error level 1 (abnormal exit)\n"
		"\n"
		" -h, [-]he[lp]             Help\n"
		"\n";
	return 0;
}

int main(int argc, char *argv[])
{
	bool asciiout = false, keepCase, inUpperCase, outUpperCase;
	base::action invact;
	base::flags flag = (base::flags)(base::INLOWER | base::OUTLOWER);
	int32_t r, _;
	uint32_t f = 0, t = 0;
	string s, fs, ts, bl = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if(argc == 1) return printHelp();
	while(--argc > 0)
	{
		s.assign(argv[argc]);
		switch(s[0] == '-' || s[0] == '/' ? ENDIAN(WC2UI16(s[1])) : ENDIAN(WC2UI16(s[0])))
		{
			case 'fr': istringstream(s.substr(3)) >> f; break; // Set from radix
			case 'tr': istringstream(s.substr(3)) >> t; break; // Set to radix
			case 'fs': r = s.find_first_of('=', 2); fs.assign(s.substr(r >= 0 ? r + 1 : 0, s.length() - 1)); break; // Set from string
			case 'ts': r = s.find_first_of('=', 2); ts.assign(s.substr(r >= 0 ? r + 1 : 0, s.length() - 1)); break; // Set to string
			case 'fn': flag = (base::flags)(flag | base::INNUMERIC); break; // From numeric/no string
			case 'tn': flag = (base::flags)(flag | base::OUTNUMERIC); break; // To numeric/no string
			case 'fl': flag = (base::flags)((flag & ~base::INLOWER) | base::INLOWER); break; // From lowercase
			case 'tl': flag = (base::flags)((flag & ~base::OUTLOWER) | base::INLOWER); break; // To lowercase
			case 'fu': flag = (base::flags)((flag & ~base::INUPPER) | base::INUPPER); break; // From uppercase
			case 'tu': flag = (base::flags)((flag & ~base::OUTUPPER) | base::OUTUPPER); break; // To uppercase
			case 'fb': case 'f2': f =  2; if(!fs.size()) fs = bl.substr(0, f); break; // From binary
			case 'tb': case 't2': t =  2; if(!ts.size()) ts = bl.substr(0, t); break; // To binary
			case 'fo': case 'f8': f =  8; if(!fs.size()) fs = bl.substr(0, f); break; // From octal
			case 'to': case 't8': t =  8; if(!ts.size()) ts = bl.substr(0, t); break; // To octal
			case 'fd': case 'f1': f = 10; if(!fs.size()) fs = bl.substr(0, f); break; // From decimal
			case 'td': case 't1': t = 10; if(!ts.size()) ts = bl.substr(0, t); break; // To decimal
			case 'fh': case 'fx': f = 16; if(!fs.size()) fs = bl.substr(0, f); break; // From hexadecimal
			case 'th': case 'tx': t = 16; if(!ts.size()) ts = bl.substr(0, f); break; // To hexadecimal
			case 'ft': case 'fH': case 'f3': fs = bl.substr(0, f = 36); break; // From hexatrigesimal (base 36)
			case 'tt': case 'tH': ts = bl.substr(0, t = 36); break; // To hexatrigesimal (base 36)
			case 'fa': case 'fe': fs.resize(256); for(_ = 0, f = 256; _ < 256; _++) fs[_] = (unsigned char)_; break; // Ascii/binary input mode
			case 'ta': case 'te': ts.resize(256); for(_ = 0, asciiout = true, t = 256; _ < 256; _++) ts[_] = (unsigned char)_; break; // Ascii/binary output mode
			case 'ig': case 'sk': case 'dr': case 'co': invact = base::IGNORE; break; // Ignore invalid characters in input (default)
			case 'ze': case '0\0': invact = base::ZERO;   break; // Zero invalid characters in input
			case 'qu': case 'X\0': case 'x\0': invact = base::QUIT;   break; // Quit read on invalid characters in input
			case 'in': case 'se': case 'al': case 're': invact = base::INFORM; break; // Inform on stderr invalid characters in input
			case 'ex': case 'er': case 'el': invact = base::EXIT;   break; // Exit (with error level) on invalid characters in input
			case 'h\0': case '?\0': default: return printHelp(); // Help
		}
	}
	if(f < 2) { cout << "From radix not specified! " << endl; return 1; }
	if(t < 2) { cout << "To radix not specified! Perhaps you specified from radix twice?" << endl; return 1; }
	base b(f, t, fs, ts, flag, invact);
	string data(b.readsize(), '\0');
	for(cin.read(&data[0], b.readsize()); r = cin.gcount(); cin.clear(), data.resize(b.readsize()), cin.read(&data[0], b.readsize()))
	{
		data.resize(r);
		b << data;
		b >> data;
		if(MIN(data.size(), b.writesize()))
		{
			for(_ = b.writesize() - data.size(); _ > 0; _--) cout << b.zero(); // Zero pad
			cout.write(&data[0], data.size());
		}
	}
	cout << flush << flush;
	if(!asciiout) cout << endl;
}
