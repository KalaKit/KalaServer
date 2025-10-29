# Construction

```cpp
s8<8> username{};                       //zero-initialized (all characters set to 0)
s8<8> userA = "admin";                  //from string literal
s8<8> userB = 'A';                      //from single character
s8<8> userC = "guest";                  //from const char*
s8<8> userD = string("root");           //from string
```

# Assignment

```cpp
userA = "root";                         //assign string literal (zero-filled)
userA = "abc";                          //shorter string overwrites and pads
userA = userB;                          //copy assignment
userA = string("user");                 //assign from string
```

# Conversion

```cpp
string raw       = userA.to_string();   //convert to string
const char* ptr  = userA.c_str();       //get null-terminated C-string
const char* data = userA.data();        //raw data pointer
```

# Size and state

```cpp
usize totalSize = userA.size();         //total capacity (fixed length)
usize realLen   = userA.length();       //actual character count before null
bool isEmpty    = userA.empty();        //true if string is empty
userA.clear();                          //clears to all zeros
```

# Search and match

```cpp
if (userA.starts_with("adm")) {}       //check prefix
if (userA.ends_with("min")) {}         //check suffix
if (userA.find('m') != userA.size()) {}//find character
if (userA.contains('r')) {}            //contains character
```

# Substring and resize

```cpp
auto slice = userA.substr(1, 3);        //substring of 3 chars starting at index 1
userA.truncate(3);                      //cut off after index 3
userA.resize(6, '_');                   //pad to length 6 with '_'
```

# Trim

```cpp
userA.trim_start("adm");               //remove prefix if matches
userA.trim_end("min");                 //remove suffix if matches
userA.trim("user");                    //remove both prefix and suffix
```

# Append

```cpp
userA += userB;                         //append another FixedString
```

# Comparisons

```cpp
if (userA == userB) {}                 //FixedString == FixedString
if (userA != userB) {}                 //FixedString != FixedString
if ((userA <=> userB) == 0) {}         //FixedString <=> FixedString

if (userA == string("root")) {}        //FixedString == string
if (userA != string("guest")) {}
if ((userA <=> string("admin")) < 0) {}

if (string("admin") == userA) {}       //string == FixedString
if ((string("admin") <=> userA) > 0) {}
```

# Access and iteration

```cpp
char first = userA[0];                 //index access (non-const)
userA[0] = 'X';                        //index assignment

for (char ch : userA)                  //iteration over content
{
	if (ch == 0) break;                //stop at null terminator
	// process ch
}
```
