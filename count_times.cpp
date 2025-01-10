
// program to read a timeline file (.txt) and accumulate activity times of each category, and output its result

#include<iostream>
#include<iomanip>		// for std::get_time()
#include<ctime>			// for std::tm
#include<sstream>		// for istringstream
#include<vector>		// for std::vector
#include<stdexcept>		// for error types such as std::invalid_argument()
#include<fstream>		// for ifstream to read a file
#include<regex> 		// for regex use inside operator>>()

/*
  Assumed timeline format:
  - w ~19:20 read manga (w: wasteful activities)
  - t 20:00 task 1      (t: task)
  - m ~21:00 ate lunch  (m: miscellaneous activities like cooking, eating, etc)
  ...

  - type (~)24-hour-format time activity detail

  on 9/14,2024, I add a new feature: mix of different activities, e.g.
  - h+w ~21:00 ate dinner, watching YouTube (W ~20m)
  The program consideres h (housechore) as the main activity, and the following w (wasteful) as a secondary activity, and
  looks for the anotation w in the timeline sentences enclosed with parenthesis (w ), followed by the length in time (~20m) 
  or the time stamps (e.g. ~20:40 - ~20:50)
*/

using namespace std;

tm date;
// date for the current file
// made global for operator>>(istream& is, Timeline& t) to access it

enum class activity_type {not_set=1, task, wasteful, house_chore, social, write_log,
			  miscellaneous, exercise, travel, rest, pastime, error};
// Since this enum class is defined inside the class declaration, this enum class can be used only inside
// this class. <- no, if this is defined in the private region, this is true, but if this is defined in the
// public region, this can be used from outside of this class (e.g. Timeline::activity_type::tast)
// Since the first element is assigned 1, int(rest) indicates the number of elements in this enum class.
// <- later, to make this enum class accessible to Abst_Timeline and Sub_Timeline, I placed this enum class outside of
//    Timeline class.
// "error" activity_type is used to indicate that a given character in convert_c2a() is not any of the assigned characters

// Let's define an abstract class for Timeline and sub_activity
// The merits of defining this abstract class:
// We readers of this program can see the connection between Timeline and sub_activity
// (both have a time stamp and an activity_type),
// we can let these child (derived) classes have the same base structure
// From ref: https://www.simplilearn.com/tutorials/cpp-tutorial/abstract-class-in-cpp#:~:text=Why%20do%20we%20use%20abstract,promoting%20code%20reuse%20and%20polymorphism.
// An abstract class is used to enforce a common interface (what member functions are prepared, how they are defined)
// to all derived classes.
// Also, abstract classes can have the usual benefits of inheritance: avoiding repetition of
// the same code, runtime polymorphism (a derived class can be passed to an argument of the
// base class type, and when the function refers to a virtual function of the base class,
// the compiler refers to the virtual function table)

// An abstruct class to impose a common interface of having a struct tm and activity_type a (and methods to access them from
// outside) on derived classes.
struct Abst_Timeline {
  // public region. Anyone can access these, but since Abst_Timeline cannot be instantiated on its own, users cannot use them
  // with Abst_Timeline instance.
  // These have to be used from the derived classes
  
  //const struct tm& get_tm(){return end_t;}
  // to prevent modification of end_t, return as a const reference
  // -> I decided to let end_t be modified in the main while-loop, so I remove this function and
  //    made end_t public.
  struct tm end_t;			// time stamp of this timeline (end time of this activity)
  
  const activity_type& get_a() const {return a;};
  // To let the compiler know that this member function doesn't modify member a,
  // set this function to a const member function (otherwise the compiler emits an error in
  // main()'s part that subtracts sub-activity's duration from main activity's duration)

  // delete copy constructor and assignment operator of an abstract class to prevent "slicing"
  // (when Abst_class = derived_class happens, some additional members in derived_class are lost)
  // (p503 in PPPC)
  //Abst_Timeline(const Abst_Timeline&)=delete;
  //Abst_Timeline& operator=(const Abst_Timeline&)=delete; // an implicit first argument is Abst_Timeline itself (*this)
  // I think placing them in protected region is sufficient, rather than deleting them, because making them protected
  // allows the derived classes to use these copy constructor and copy assignment in the derived classes' copy constructor.
  // However, I will keep them deleted as advised in p504, as it says there are other reasons why it's better to keep them
  // deleted (I don't know what these reasons are).
  // -> to use the copy constructor in Timeline's copy constructor, I decided to put them in the protected region below.
  //    Placing them in the protected region also prohibits copying outside this class tree (base class + derived classes)

  // virtual destructor. Needed in case its derived class allocates some resources and the DERIVED class's destructor
  // needs to be called when the derived class is made with new (see section 17.5.2 of PPPC for the detail).
  virtual ~Abst_Timeline(){}

  int task_num;
  // used only when classifying activity_type::task. Both a main activity (Timeline class) and a sub-activity (Sub_Timeline)
  // can have this task number.
  // For classifying a main activity, operator>>(istream& is, Timeline& t) searches for a string such as
  // "task 1" in the activity_content. In that case, task_num == 1.
  // For classifying a sub-activity, it searches for a digit following the sub-activity label. For example, "(t2 ~20m)",
  // in which case task_num == 2.
  // When task_num is 0, that means either the activity_type is not task, or if it's a task, the task is missing
  // a task number, hence "unclassified" task.
  
protected:
  // protected region is not accessible by outside this class, but accessible by classes derived from this class,
  // whereas private region is only accessible by the class that defines it.
  // ref: https://stackoverflow.com/questions/224966/what-is-the-difference-between-private-and-protected-members-of-c-classes
  // I want my derived classes to access the base class's constructor (in their constructors), so I put the constructors of
  // Abst_Timeline into protected region.
  Abst_Timeline(): end_t{}, a{activity_type::not_set}, task_num{0} {}

  Abst_Timeline(activity_type act): end_t{}, a{act}, task_num{0} {}
  
  // Instead of deleting these copy constructor and assignment operator, I decided to put them in this protected region,
  // to use these in derived classes' constructor
  Abst_Timeline(const Abst_Timeline& atl) : end_t{atl.end_t}, a{atl.a}, task_num{atl.task_num} {}
  Abst_Timeline& operator=(const Abst_Timeline& atl){
    end_t = atl.end_t; a = atl.a; task_num = atl.task_num;
    return *this;
  }
  
  activity_type a;
  // to make these members accessible to derived classes, but not to outside derived classes, I made them protected
  
private:
  // When I made end_t and a private, even the derived classes couldn't access them, which caused errors in operator>>() overload
};
// To define an abstract class we need to either:
//  - have at least one pure virtual function (virtual return_type func_name(args...) = 0;)
//  - make the constructor hidden (either delete them or hide them in protected: region)
// to make it impossible to instantiate an abstract class on its own.

string convert_a2s(activity_type); // forward declaration to use it in Sub_Timeline::print_subact().

// Let's define a class for sub activities inside a Timeline
struct Sub_Timeline : Abst_Timeline {
  Sub_Timeline() : Abst_Timeline{}, duration{} {}
  Sub_Timeline(activity_type a) : Abst_Timeline(a), duration{} {}

  // When all the three conditions below are met, an implicit copy constructor (member-wise copy) is created in a derived
  // class by compiler (by ChatGPT):
  // 1. No explicit copy constructor is defined in the derived class: If you explicitly define a copy constructor
  //    in the derived class, the compiler will not generate one implicitly.
  // 2. The base class has a copy constructor that is accessible: The compiler needs to be able to call the copy
  //    constructor of the base class when copying the derived class. If the base class copy constructor is private
  //    or deleted, the compiler cannot generate an implicit copy constructor for the derived class.
  // 3. All member variables in the derived class are copyable: The compiler will generate an implicit copy constructor
  //    only if all non-static data members of the derived class can be copied using their own copy constructors.
  // In this case, since the copy constructor of the base class Abst_Timeline() is accessible from this Sub_Timeline,
  // an implicit copy constructor of Sub_Timeline is created by the compiler.
  
  //struct tm start_t;
  unsigned int duration; // [minutes]
  // start_t might not be specified. In that case, leave it blank.
  // To check if start_t is empty, I can check start_t.tm_mday is 0 or not.
  // ref: https://cboard.cprogramming.com/c-programming/179972-how-check-struct-tm-variable-empty.html
  // about memset(): https://cplusplus.com/reference/cstring/memset/

  void print_subact(){
    cout << convert_a2s(a) << ", duration = " << duration << endl;
  }
private:
  // I made the member variables public
};

// Let's define a class that specializes in reading each timeline
struct Timeline : Abst_Timeline {
  Timeline(string s);
  // if a constructor is defined explicitly, implicit default constructor is deleted

  //Timeline() : end_t{}, a{activity_type::not_set} {}
  Timeline() : Abst_Timeline{} {}
  
  void print_tl();		// for checking what's read

  // explicitly define a copy constructor and an assignment operator for Timeline because they are deleted
  // in Abst_Timeline
  // They are needed when I use vector<Timeline>::push_back()
  //Timeline(const Timeline& tl) : end_t{tl.end_t}, a{tl.a}, activity_content{tl.activity_content} {}
  // <- this caused an error. It seems a base class's members cannot be initialized with a derived class' initializer list
  // ref: https://stackoverflow.com/questions/18479295/member-initializer-does-not-name-a-non-static-data-member-or-base-class
  // -> So I made Abst_Timeline's copy constructor protected.
  // <- but when I do so, since derived classes' copy constructor is not deleted anymore, I don't need to explicitly
  //    define these copy constructor and assigment operator anymore.
  Timeline(const Timeline& tl) : Abst_Timeline(tl), activity_content{tl.activity_content} {}
  // Abst_Timeline(tl) uses Abst_Timeline's protected copy constructor. Since tl (Timeline) is a kind of Abst_Timeline,
  // passing a Timeline to Abst_Timeline doesn't generate an error.
  Timeline& operator=(const Timeline& tl){
    end_t = tl.end_t; a = tl.a; activity_content = tl.activity_content;
    return *this;
  }

  // I prepare these functions because I want to keep subtl_vec private, because they are set in
  // iss >> Timeline, and once it's read this way then the subtl_vec is modified, it's hard to recover the original one.
  // So I wanted to protect subtl_vec, imagining that this Timeline class is provided as a library class and
  // many different users use this Timeline class.
  // But this is just my current thought. Feel free to change it.
  const Sub_Timeline& get_subtl(int i){return subtl_vec[i];}
  int get_subtl_size(){return subtl_vec.size();}
  
private:
  
  friend istream& operator>>(istream& is, Timeline& t);
  // make this overloaded operator friend to this class because I want it to access Timeline's
  // private members. This friend declaration can be done anywhere in class declaration

  string activity_content;

  // in case some sub timelines are included in a Timeline
  // e.g. - H+w ~19:20 ate dinner, watched YouTube afterward (w ~20m)
  vector<Sub_Timeline> subtl_vec;
};
// using struct, instead of class, makes its members public by default.
// When inheriting a struct class, its inheritance mode becomes public by default.
// When inheriting a class class, its inheritance mode become private by default

// to reuse this part in reading time stamps in timeline sentences, I made this part in operator>>() a function
// Read a time stamp (e.g. ~19:20) from istream and put it in tm
istream& read_timestamp(istream& is, struct tm& t, bool RecoverTimeChars = false){
  // RecoverTimeChars: if true, when reading a time stamp with is >> get_time() failed, it moves the
  // position of is reading and recovers the read time chars. Used for the following read_duration()
  // for sub-activities.
  
  char c;
  
  is >> c;
  if(c != '~' && isdigit(c)){			// case without '~' e.g. "19:20"
    is.unget();					// put back the last character (digit c)
    // I think istream remembers what it emits lastly.
    // is.putback(c); does the same thing
  }
  else if(c == '~'){	     // case with '~' e.g. "~19:20"
    // check if the next character is a digit
    is >> c;
    if(!isdigit(c)){
      cerr << "Error in reading a timeline: after activity types, missing time e.g. (~)19:20\n";
      is.clear(ios_base::failbit);
      // is.unget();		// for a debug purpose, put back the last character
      return is;
    }
    is.unget();			// put back the digit
  }
  else{
    cerr << "Error in reading a timeline: after activity types, missing time e.g. (~)19:20\n";
    is.clear(ios_base::failbit);
    return is;
  }

  // read the time ref: https://stackoverflow.com/questions/17681439/convert-string-time-to-unix-timestamp
  streampos pos = is.tellg(); // Save the current stream position to get back to it if failed
  is >> get_time(&t, "%H:%M");	// https://en.cppreference.com/w/cpp/io/manip/get_time
  // I wonder how operator>> can be overloaded with function get_time().
  // Maybe it overloads >> with istream& and the return type of get_time()?
  if(is.fail()) {
    //cerr << "Error in reading a timeline time\n";
    // In reading a sub-acitivity duration (e.g. (w ~10m)), it fails and enters this if-block.
    // In such a case, just returns is without false_bit being set

    if(RecoverTimeChars){
      is.clear(); // remove failbit
      // When failbit is set, is.tellg() always returns -1, and we cannot set the position with is.seekg(pos),
      // so we need to clear the failbit before using is.seekg(pos);
      is.seekg(pos);           // Go back to the saved position (by ChatGPT)

      // this istream::seekg(pos) (and istream::seekp(pos) for positioning the writing pointer) works only when
      // istream is a file stream (e.g. ifstream). A normal istream like cin discards already read characters
      // from the streambuf (buffer) except for the last character (for .unget() or similar functions), so this
      // .seekg() method doesn't work for them. In this case, since we read a timeline file, this .seekg(pos) method
      // works properly (ChatGPT and section 11.3.3 of "Programming Principles and Practices Using C++").
      
      // return is to failbit being set, as in the reading of sub-activity, I want the first if-condition to be false
      is.clear(ios_base::failbit);
    }

    return is;
  }

  return is;
}

// read a time duration e.g. ~15m, for sub-activities
istream& read_duration(istream& is, struct tm& t){
  // read_duration() is used after read_timestamp() failed in operator>>().
  // So, is' failbit is set already.
  // Thus, first clear the failbit
  is.clear(); // remove failbit
  
  char c;
  
  is >> c;
  if(c != '~' && isdigit(c)){			// case without '~' e.g. "19:20"
    is.unget();					// put back the last character (digit c)
    // I think istream remembers what it emits lastly.
    // is.putback(c); does the same thing
  }
  else if(c == '~'){	     // case with '~' e.g. "~20m"
    // check if the next character is a digit
    is >> c;
    if(!isdigit(c)){
      cerr << "Error in reading a timeline: after activity types, missing time e.g. (~)15m\n";
      is.clear(ios_base::failbit);
      // is.unget();		// for a debug purpose, put back the last character
      return is;
    }
    is.unget();			// put back the digit
  }
  else{
    cerr << "Error in reading a timeline: after activity types, missing time e.g. (~)15m\n";
    is.clear(ios_base::failbit);
    return is;
  }

  int num;
  is >> num;
  if(is.fail()) {
    return is;
  }

  // This part needs modification to accept a case like 1h20m.
  // Currently, this part reads '1' in num and "h20m" in time_qual in the current implementation.
  
  // obtain the following time-qualifier e.g. m/min/minutes for minutes, h/hour/hours for hours
  string time_qual;
  if(!getline(is, time_qual, ')')){
    // read is until it reaches the specified delimiter ')'. The reached delimiter is discarded
    // (not stored) and reading starts from the next char.
    // https://cplusplus.com/reference/string/string/getline/

    // getline() returns istream&. if(istream) uses operator bool().
    // operator bool() returns true unless istream::fail() is true.
    // Even when eof() is true, if fail() is not true, istream returns true.
    // https://en.cppreference.com/w/cpp/io/basic_ios/operator_bool
    // So entering this if-block means the read in getline() somehow failed.
    return is; // is' failbit is set
  }
  if(time_qual == "m" || time_qual == "min" || time_qual == "mins" || time_qual == "minute" || time_qual == "minutes"){
    t.tm_min = num;
    // In struct tm, the range of tm_min is 0-59, but since it's just an int, it can be set to outside of this rance
  }
  else if(time_qual == "h" || time_qual == "hr" || time_qual == "hrs" || time_qual == "hour" || time_qual == "hours"){
    t.tm_hour = num;
  }
  else{
    cerr << "Error in reading a timeline: after activity types, missing a correct time qualifier (m/min/mins/minute/minutes/h/hr/hrs/hour/hours) e.g. (~)15m\n";
    is.clear(ios_base::failbit);
    return is;
  }  
  
  return is;
}

// forward declaration as set_dates() is used inside read_sub_timestamp()
tm set_dates(tm ref_date, tm* b_tm, tm* e_tm);

// read sub-activity's time stamp (either ~20m or 19:00 - ~19:20) and store it in Sub_Timeline::duration.
// If the time stamp is in the latter form and the start/end times are available, this func. also stores Sub_Timeline::end_t.
istream& read_sub_timestamp(istream& is, Sub_Timeline& subtl){
  char ct{0};
  tm b_tm{}, e_tm{};
  // make sure to initialize b_tm and e_tm, because otherwise some unused fields like tm_sec is set to a random
  // value, which causes b_tm and e_tm to be modified to a strange date in mktime() inside set_dates().
  tm duration{};
  if(read_timestamp(is, b_tm, true)){ // e.g. ~19:20, ~20m fails and goes to the next if-block
    // after entering here, we expect e.g. "- ~20:15"
    is >> ct;
    //cout << "### " << asctime(&b_tm) << endl;
    //cout << "### ct = " << int(ct) << endl;
    //cout << "### int('-') = " << int('-') << endl;
    // A seemingly same character '-' caused an error. An en-dash was used in the place of a dash
    // '-' (ASCII hyphen, code point 45) is different from – (en-dash, code point 8211) or — (em-dash, code point 8212)
    if(ct != '-'){
      cerr << "Error in reading the beginning/end time stamps of a sub-activity. The format is e.g. \"(~)19:20 - (~)20:15\"" << endl;
      cerr << "Also, check the hyphen. This program accepts only ASCII hyphen '-' (code 45), but your text might be using different hyphens like – (en-dash, code point 8211) or — (em-dash, code point 8212)" << endl;
      is.clear(ios_base::failbit);
      return is;
    }
    if(!read_timestamp(is, e_tm, false)){
      is.clear(ios_base::failbit);
      return is;
    }
	    
    // derive duration from b_tm and e_tm
    // Since the year, month, and day are not set, the way used in the main()'s while loop (convert tm to time_t and take 
    // the difference) doesn't work.
    // <- I want to deal with a similar case to the main() function like b_tm is 23:50 of 2024/11/30 and e_tm is
    //    0:20 of 2024/12/1. So I will employ the same strategy as in the main() function.
    set_dates(date, &b_tm, &e_tm);
    // This date is a global variable defined at the top of this program.
    // Don't update the global "date" variable here, because if it were updated here, it affects the calculation of
    // main()'s b_tm, e_tm in while-loop.
    // e.g.
    // 11/30/2024
    // - t 23:50 task 1
    // - t+w 0:20 ... (w 23:55-0:15).
    // Processing sub-activities happens before processing Timeline time stamps in main()'s while-loop.
    // When processing sub-activities in t+w 0:20 timeline, if the global "date" variable is updated here, t 23:50
    // is also regarded as 23:50 on 12/1, and t+w 0:20 is updated to 12/2/2024 in set_dates() inside the main()'s while
    // loop (still, the computation should be performed correctly, but the date is shifted by one).
	    
    time_t b, e;
    b = mktime(&b_tm);
    e = mktime(&e_tm);

    double seconds = difftime(e,b);
    subtl.duration += seconds/60; // seconds/60 [minutes]

    subtl.end_t = e_tm; // this is not necessary, but just to store all available info
  }
  else if(read_duration(is, duration))
    subtl.duration += duration.tm_min;

  return is;
}


// helper function to convert activity_type to the corresponding char
char convert_a2c(activity_type a){
  switch(a){
  case activity_type::wasteful:
    return 'w';

  case activity_type::task:
    return 't';

  case activity_type::miscellaneous:
    return 'm';

  case activity_type::house_chore:
    return 'h';

  case activity_type::social:
    return 's';

  case activity_type::write_log:
    return 'l';

  case activity_type::exercise:
    return 'e';

  case activity_type::rest:
    return 'r';

  case activity_type::travel:
    return 't';

  case activity_type::pastime:
    return 'p';

  case activity_type::not_set:
    return 'n';

  case activity_type::error:
    return 'z';

  default:
    throw runtime_error("Error in convert_a2c(), unknown activity_type a is passed");
  }
}


// helper function to convert a char to the corresponding activity_type
activity_type convert_c2a(char c){
  switch(c){
  case 'w':
    return activity_type::wasteful;

  case 't':
    return activity_type::task;

  case 'm':
    return activity_type::miscellaneous;

  case 'h':
    return activity_type::house_chore;

  case 's':
    return activity_type::social;

  case 'l':
    return activity_type::write_log;

  case 'e':
    return activity_type::exercise;

  case 'r':
    return activity_type::rest;

  case 'd':
    return activity_type::travel;

  case 'p':
    return activity_type::pastime;

  case 'n':
    return activity_type::not_set;
  }

  return activity_type::error;			// "error" represents an unknown activity type
}

// helper function to convert an activity type to a string (usually for debug)
string convert_a2s(activity_type a){
  switch(a){
  case activity_type::wasteful:
    return "Wasteful activity";

  case activity_type::task:
    return "Task";

  case activity_type::miscellaneous:
    return "Miscellaneous";

  case activity_type::house_chore:
    return "House chores";

  case activity_type::social:
    return "Social activity";

  case activity_type::write_log:
    return "Write log";

  case activity_type::exercise:
    return "Excercise";

  case activity_type::rest:
    return "Rest";

  case activity_type::travel:
    return "Travel";

  case activity_type::pastime:
    return "Pastime";

  case activity_type::not_set:
    return "Not set";

  case activity_type::error:
    return "Activity type Error";

  default:
    throw runtime_error("Error in convert_a2s(), unknown activity_type a is passed");
  }
}


// based on the first argument tm ref_date, update b_tm and e_tm's date (year, month, day) info.
// This deals with a bridging case like b_tm = 23:50 on 11/30/2024 and e_tm = 0:20 on 12/1/2024.
// Returns an updated ref_date. To avoid updating the global "date" variable (because global date
// is for calculating the difference between the main activities, while local date updates may be necessary
// when calculating the difference between sub-activities), I made ref_date local to this function.
tm set_dates(tm ref_date, tm* b_tm, tm* e_tm){

  time_t b,e;

  //cout << "### ref_date = " << asctime(&ref_date) << endl;
  
  // cout << "### b_tm.tm_year = " << b_tm->tm_year << endl; // 0
  // cout << "### b_tm.tm_mon = " << b_tm->tm_mon << endl;   // 0
  // cout << "### b_tm.tm_mday = " << b_tm->tm_mday << endl; // 0
  // cout << "### b_tm.tm_hour = " << b_tm->tm_hour << endl; // 19
  // cout << "### b_tm.tm_min = " << b_tm->tm_min << endl;   // 20
  b_tm->tm_year = ref_date.tm_year;
  b_tm->tm_mon = ref_date.tm_mon;
  b_tm->tm_mday = ref_date.tm_mday;
  b_tm->tm_sec = 0;
  // When I didn't set the year, month, and day of the tm object, mktime() returned -1
  // (I think since Timeline tl is initialized with the default constructor, tm is initialized with {},
  // which makes tl.end_t.tm_sec 0. So I don't have to explicitly set b_tm.tm_sec to 0)
  b_tm->tm_wday = 0;
  b_tm->tm_yday = 0;
  b_tm->tm_isdst = 0;
  
  //cout << "### In set_dates(), b_tm BEFORE mktime() = " << asctime(b_tm) << endl;
  
  //b = mktime(&const_cast<struct tm&>(tl_vec[c-2].get_tm())); // mktime(struct tm*) returns corresponding time_t object
  b = mktime(b_tm);
  // since mktime(struct tm*) doesn't accept const struct tm*, I strip constness away by using const_cast<>
  // When I const_cast to <struct tm> (without reference), that causes an error.
  // mktime(tm* time) modifies the argument time so as to make each field have a valid range (e.g. if tm_sec is -10,
  // then mktime() would modify tm_sec to 50, and decrease tm_min by one minute).
  // To avoid an unexpected modification of time, make sure to initialize time with 0, setting each field 0.
  // -> to prevent such an unexpected modification, I added initialization lines to b_tm and e_tm above and below
  //    for tm_wday, tm_yday, tm_isdst.
  // (mktime(tm* time) modifies these fields (tm_wday, tm_yday, tm_isdst) appropriately according to tm_year, tm_mon,
  // tm_mday, tm_min, tm_sec)

  //cout << "### In set_dates(), b_tm AFTER mktime() = " << asctime(b_tm) << endl;
  
  /*
    char buff[10];		// container to have time e.g. "19:27"
    strftime(buff, sizeof(buff), "%H:%M", &const_cast<struct tm&>(tl_vec[c-2].get_tm()));
    cout << "Time stamp: " << buff << endl;
    tm tm1;
    tm1.tm_year = 2020 - 1900; // 2020
    tm1.tm_mon = 2 - 1; // February
    tm1.tm_mday = 15; // 15th
    tm1.tm_hour = tl_vec[c-2].get_tm().tm_hour;
    tm1.tm_min = tl_vec[c-2].get_tm().tm_min;
    cout << "### tl_vec[c-2].get_tm().tm_hour = " << tl_vec[c-2].get_tm().tm_hour << endl;
    cout << "### tl_vec[c-2].get_tm().tm_min = " << tl_vec[c-2].get_tm().tm_min << endl;
    tm1.tm_sec = 0;
    // When tm1.tm_sec is not set explicitly to 0, tm_sec value takes a random value since
    // tm1 is not initialized.

    cout << "tm1: " << std::put_time(&tm1, "%c %Z") << '\n';
    time_t k = mktime(&tm1);
    tm local = *std::localtime(&k);
    cout << "local: " << std::put_time(&local, "%c %Z") << '\n';
  */

  // set the date
  e_tm->tm_year = ref_date.tm_year;
  e_tm->tm_mon = ref_date.tm_mon;
  e_tm->tm_mday = ref_date.tm_mday;
  e_tm->tm_sec = 0;
  // When I didn't set the year, month, and day of the tm object, mktime() returned -1
  // (I think since Timeline tl is initialized with the default constructor, tm is initialized with {},
  // which makes tl.end_t.tm_sec 0. So I don't have to explicitly set e_tm.tm_sec to 0)
  // <- after I set this part as a separate function set_dates(), set_dates() may take tm objects not properly
  //    initialized. For that case, set tm_sec to 0.
  //    If tm_sec has some random bits like -1222411104, mktime() modifies e_tm object to make each field's range
  //    valid (for the valid range, see https://cplusplus.com/reference/ctime/tm/)
  e_tm->tm_wday = 0;
  e_tm->tm_yday = 0;
  e_tm->tm_isdst = 0;

  //cout << "### In set_dates(), e_tm before mktime() = " << asctime(e_tm) << endl;

  //e = mktime(&const_cast<struct tm&>(tl_vec[c-1].get_tm()));
  e = mktime(e_tm);
  // cout << "### b = " << b << endl;
  // cout << "### e = " << e << endl;

  //cout << "### In set_dates(), e_tm AFTER mktime() = " << asctime(e_tm) << endl;
  
  // time_t (b and e) is expressed in int internally since 0:00, Jan 1, 1970. So I can add an int to it
  // cout << "### e+200 = " << e+200 << endl;
  if(e < b){	
    // when e (end time) is less than b, there are two possibilities: one is the time record is wrong, the other is
    // the ref_date becomes the next day (e.g. 23:59 on 9/2/2024 -> 0:15 on 9/3/2024).
    // (although it's 0:15 on 9/3/2024, since this program doesn't know that the ref_date moved to 9/3, it regards
    // 0:15 as 0:15 on 9/2, so e becomes less than b when the ref_date changes).
    // When it is a mistake of time record, it is likely that only the minute part is reversed,
    // e.g. b 23:51, e 23:40
    // So let's assume this: when b-e < 60*60 (1 hour), I assume this is a mistake of time record, and output an error message,
    // otherwise I assume the ref_date is updated to the next day.
    if(b-e<60*60){
      cerr << "Error in reading a timeline at time " << put_time(localtime(&e), "%c %Z") << endl;
      cerr << "The next time stamp is before the target time stamp." << endl;
      throw runtime_error("Runtime Error");
    }

    // reaching here means b is still in the previous date, but e is in the next date
	
    // simply adding 1 to date.tm_mday can make an error (e.g. when it is 31, adding 1 to it causes it to become 32).
    // So as I tested in test_time_funcs.cpp, first convert it to time_t, add 60*60 to it, then convert it back to
    // tm.
    time_t date_t = mktime(&ref_date); // unlike localtime(time_t*) and gmtime(time_t*), mktime returns the time_t object itself
    // (they return the pointer to a tm object made in static memory)
    date_t += 60*60*24;	// forward it by one day in seconds
    tm* date_ptr = localtime(&date_t); // localtime() adds the local time zone info.
    // the corresponding function with UTC is gmtime()
    // Since localtime() and gmtime() returns a pointer to the tm object made in static memory, I need a pointer to it

    // update date
    ref_date = *date_ptr;	// copy the tm object in static memory to the local tm variable

    // after updating date, update e the same way as above
	
    e_tm->tm_year = ref_date.tm_year;
    e_tm->tm_mon = ref_date.tm_mon;
    e_tm->tm_mday = ref_date.tm_mday;
    // Be careful not to use date. use ref_date.
    
    //cout << "### set_dates(), date has changed, e_tm = " << asctime(e_tm) << endl;
  } // if(e < b){
  
  return ref_date;
}

// Get the task number following a task label (either main activity type or a sub-activity type).
// Assumed task number patters:
// 1. The task digit follows the task label ('T') in the main activity
//   - T1+w 19:00 did task 1 (did a unit test). watched YouTube (w ~20m).
//
// 2. The task digit follows the sub-task label ('+t')
//   - H+t2+w 20:00 did a house chore. did task 2 (did a unit test) (t ~19:20-19:50). Read manga (w ~5m)
//   - H+t2 20:00 cleaned house. did task 2 (did a unit test) (t ~30m)
//
// 3. The task digit follows a sub-task label in the activity content inside parentheses.
//   - H+t 14:00 cleaned house. did task 2 (t2 ~13:30 - 14:00).
//
// 4. The task digit does not exist in the activity type list in the first part. We have to find it in the
//    activity content, such as "task 1".
//   - T 15:30 did task 3 (did a unit test).
//   - H+t 16:00 cleaned house. did task 2 (t ~20m)
//
// - T+t+w 19:00 did task 1 (did a unit test). did task 2 (t ~20m).
// In get_task_num(), it deals with cases 1-3. Case 4 is dealt with separately.
istream& get_task_num(istream& is, Abst_Timeline& t){
  // Both Timeline and Sub_Timeline are derived from Abst_Timeline, so we can pass both of these
  // derived classes to the second argument.
  
  char c;
  // search for a task digit if any
  //is >> c;
  is.get(c);
  // istream::get() is used so that it doesn't skip whitespaces. The task digit must follow the task label
  // ('t') without any whitespace. This definition is helpful to distinguish whether the following digit is
  // for the time stamp following the task label e.g. - t 19:20
  if(isdigit(c)){
    t.task_num = c - '0';
  }
  else
    is.putback(c);
  return is;
}

// Define this operator overload outside the class, because if I define it inside
// the class, the first argument is automatically determined to be Timeline (this) (implicit argument).
// So I need to define this outside the class
istream& operator>>(istream& is, Timeline& t){
  is.exceptions(is.exceptions()|ios_base::badbit); // make is throw if it goes bad
  
  // first, I need to check if '-' is present
  is >> ws;			// skip whitespaces (https://cplusplus.com/reference/istream/ws/)
  char c;
  is >> c;
  if(c!='-'){
    cerr << "Error in reading a timeline: missing \'-\' at the front\n";
    is.clear(ios_base::failbit);			// set the fail bit
    // https://cplusplus.com/reference/ios/ios/clear/
    return is;
  }

  is >> ws;

  // format of activity types: H+w+r 
  is >> c;

  /*
    a
    b
    c
    d - travel (from 'd'rive)
    e - exercise
    f
    g
    h - house_chore
    i
    j
    k
    l - write_log
    m - miscellaneous
    n - not_set
    o
    p - pastime (synonym of hobby)
    q
    r - rest
    s - social
    t - task
    u
    v
    w - wasteful
    x
    y
    z - unknown (representing an error)
  */

  // ####### section 1
  c = tolower(c);		// to accept both upper and lowercase character, convert it to lowercase
  // If c is already a lowercase character, tolower() doesn't do anything.
  // ref: https://en.cppreference.com/w/cpp/string/byte/tolower

  activity_type act = convert_c2a(c);
  if(act == activity_type::error){
    cerr << "Error: Unknown activity type \'" << c << "\' is specified\n";
    is.clear(ios_base::failbit);
    return is;
  }
  t.a = act;

  //is >> ws; // istream skips whitespaces by default, so no need to do this operation

  // Task digit type 1 (in the comments above get_task_num()'s definition)
  // search for a task digit for the main activity (the leading activity_type, e.g. T1 in "T1+w"), if any
  if(act == activity_type::task){
    get_task_num(is, t); // This func. put back the next character if the following character is not the task digit
  }

  // ######## section 2
  // check any tailing sub-activities e.g. +w+r
  is >> c;
  while(c == '+'){    
    is >> c;
    c = tolower(c);

    // when '+' precedes, the next character must be one of activity type characters below
    act = convert_c2a(c);
    if(act == activity_type::error){
      cerr << "Error: Unknown activity type \'" << c << "\' is specified\n";
      is.clear(ios_base::failbit);
      return is;
    }
    Sub_Timeline subtl(act);

    // Task digit type 2 (in the comments above get_task_num()'s definition)
    // just like the main activity_type, check if there is any task digit in sub-activities
    if(act == activity_type::task){
      get_task_num(is, subtl); // This func. put back the next character if the following character is not the task digit,
      // including when the following digit character is for the time stamp e.g. - H+t 19:20
    }
    t.subtl_vec.push_back(subtl); // using copy constructor/ assignment operator
    //t.subtl_vec.push_back(Sub_Timeline(act));
    
    is >> c; // read the next '+'
  }
  is.putback(c);		// return any character read falsely by the block above

  read_timestamp(is, t.end_t); // since read_timestamp() takes is as a reference, the change to is is reflected to this function
  // To my surprise, this compiled without error and ran correctly. t.end_t is a private member of Timeline t, and read_timestamp()
  // is not a friend function to Timeline.
  // <- I guess if read_timestamp() uses Timeline's private members inside it in the code, the compiler would generate an error.
  //    But at compile time, the compiler don't know (theoretically it can) read_timestamp() takes Timeline's private member as
  //    an argument.
  if(!is){
    // if a reading error happens in read_timestamp(), since a failbit is already set in read_timestamp(), return is
    // directly
    return is;
  }

  // if the time stamp has a tailing '?', "is" has it at the front at this point.
  
  is >> ws;			// remove any whitespaces in the front
  // This time, this is necessary because getline() includes the front whitespaces without this is>>ws;
  
  // put the rest to activity_content, including whitespaces
  getline(is, t.activity_content);
  // getline() reads until it hits a new line or char delim if you specify it

  // ########## section 2.5
  // Search the activity_content and pick any forgotten sub-activities (sub-activities not listed in the first
  // activity list (e.g. "T+w") but existent in activity_content)
  istringstream iss{t.activity_content};
  while(iss >> c){ // use the same way as section 3
    // check if this is a start of a sub-activity label.
    // e.g. (s ~20m), (t 19:00 - 19:20), (t1 18:15 - 18:30), ...
    if(c == '(' && iss >> c && convert_c2a(c) != activity_type::error){      
      activity_type act = convert_c2a(c);
      Sub_Timeline subtl(act); // at this point, it's not sure whether this is actually a sub-activity label
      
      if(act == activity_type::task){
	get_task_num(iss, subtl);
      }

      if(iss.get(c) && isspace(c)){
	// In the 1st condition, use istream::get() to capture a space (iss>>c skips whitespaces)

	// then, check if this sub-activity is not captured by t.subtl_vec (in section 2 above)
	bool captured{false};
	for(int i=0; i<t.subtl_vec.size(); ++i)
	  if(t.subtl_vec[i].get_a() == act && t.subtl_vec[i].task_num == subtl.task_num){
	    // When the activity type is not activity_type::task, task_num is always 0 (as initialized in the constructor)
	    // In terms of act == activity_type::task, the task_num also must match to be regarded as the same sub-activity type.
	    captured = true;
	    break;
	  }

	if(!captured)
	  t.subtl_vec.push_back(subtl);
      }
    }
  }
  
  // ########## section 3
  // search the activity_content for the time info (w ~10m) or (w ~15:10 - 16:20)
  for(int i=0; i<t.subtl_vec.size(); ++i){
    char ct;
    activity_type at = t.subtl_vec[i].get_a(); // .get_a() returns a const reference. copy it to the local variable at.
    istringstream iss{t.activity_content}; // to skip whitespaces simply, I put the string into istringstream
    Sub_Timeline subtl; // for a temporary storage of Sub_Timeline::task_num and Sub_Timeline::duration.
    // Do not modify t.tubtl_vec[i]'s duration and task_num yet, because at this point, this parentheses might not be
    // for a timestamp, i.e. we cannot know whether this parentheses is for example (test a program) or (t ~20m).
    subtl = t.subtl_vec[i]; // copy the activity_type a (and task_num). This variable keeps storing duration of this sub-activity
    
    // get the time info of the sub-activity.
    // This while-loop aggregates all subactivity timestamps for one sub-activity type
    while(iss >> ct){ // this automatically skips whitespaces
      // if there are some times like (w ~20m), they are aggregated.

      if(ct == '(' && iss >> ct && convert_c2a(ct) == at){
	// Task digit type 3 (in the comments above get_task_num()'s definition)
	Sub_Timeline subtl2; // just for fetching task_num in get_task_num
	if(at == activity_type::task){
	  get_task_num(iss, subtl2);
	}
	
	if(iss.get(ct) && isspace(ct) && subtl.task_num == subtl2.task_num){
	  // second condition is not executed when the first condition is false.
	  // These conditions are to check if the next character is a whitespace. This is for
	  // distinguishing the subactivity timestamps from normal parentheses e.g. (w ~10m) vs (watched YouTube).
	  // Unlike iss>>ct, which by default skips a whitespace, iss.get(ct) catches a whitespace.
	  // The first condition istream::get(char) returns the read character in int or EOF if it reaches the end of the file and
	  // doesn't read any character. <- after testing it in test_istreamget.cpp, it turns out that iss.get(c) returns
	  // istream&, not an int, as the document says.
	  // For the third condition, if both activity_types are not task, task_nums are both 0 and it becomes true.

	  // read_sub_timestamp() adds (+=) the current duration to subtl, not overwriting it (=)
	  if(!read_sub_timestamp(iss, subtl)){ 
	    is.clear(ios_base::failbit); // set is's fail bit, not iss's.
	    return is;
	  }
	} // if(iss.get(ct) && isspace(ct)){
	
      } // if(ct == '(' && iss >> ct && convert_c2a(ct) == at){
    } // while(iss >> ct){

    // at this point, all sub-activity timestamps are checked and Sub_Timeline::duration is stored.
    // If any, Sub_Timeline::task_num and Sub_Timeline::end_t are also stored.
    t.subtl_vec[i] = subtl; // copy the temporary variable
  }

  // ################ section 4
  // Task digit type 4 (in the comments above get_task_num()'s definition)
  // In the end, deal with the task digit type 4, where the task digit is not in the activity list
  // (neither "T+w" nor (t ~19:20)). In this case, we have to search for the task digit in the text.
  // I don't consider the case where there are multiple tasks in a timelne (e.g. T+t1+t3+w).
  // (in such a case, if the sub-activity time stamps have the task digits e.g. (t2 ~20m), it can store the task digit
  // with section 3 above.)
  // The list of timelines with multiple tasks sections 1-3 can deal with:
  // 1.    - T+t+w ~19:00 did task 1. did task 2 (t2 ~20m). watched YouTube (w ~10m).
  //  <- the sub-activity timestamp's "t2" makes it possible in section 3 above.
  //
  // 2.    - T+t2+w ~19:00 did task 1. did task 2 (t2 ~20m). watched YouTube (w ~10m).
  //  <- Since section 3 takes the task digit in the sub-activity, the first activity list's '2' in "t2" is
  //     not necessary.
  //
  // 3.    - T+t2+w ~19:00 did task 1. did task 2 (t ~20m). watched YouTube (w ~10m).
  //  <- "t2+w" is regarded as sub-activities. '2' is already stored in section 2 above. So even when the sub-activity
  //     timestamp doesn't have the task digit '2', the '2' is already stored, and section 3 doesn't have to catch it.
  //
  // When there is only one task in the sub-activities, section 3 works.
  //
  // The list of timelines with multiple tasks sections 1-3 can NOT deal with:
  // 4.    - T+t2+t3+w ~19:00 did task 1. did task 2 (t ~20m). did task 3 (t ~15m). watched YouTube (w ~10m).
  //  <- even when the first activity list has the task digit '2'/'3', section 3 aggregates the same activity_types
  //     in the same element, so there are two subtl_vec[i] elements with activity_type::task and both of them have
  //     15+20=35 minutes in its duration.
  //
  // 5.    - T+t2+t3+w ~19:00 did task 1. did task 2 (t2 ~20m). did task 3 (t3 ~15m). watched YouTube (w ~10m).
  //  <- even with task digits following sub-activity types, section 3 doesn't change its behavior based on the task digits,
  //     so the result is the same as above: both subtl_vec[0] and subtl_vec[1] have the aggregated time, 35 mins.
  //
  // 6.   - T+t+w 19:00 did task 1 (did a unit test). did task 2 (t ~20m).
  //      - T+w 19:00 did task 1 (did a unit test). watched YouTube (w ~20m)
  //  <- since either the activity_type list or time stamps don't have a task digit, sections 1-3 cannot find
  //     it.
  //  <- this case is dealt with by section 4 below
  //
  // Like the first two examples, when there are two or more tasks in the sub-activities, the current section 3 doesn't work.
  // This section 4 deals with the last undealt case (example 6).
  //
  // This confusing state is caused by the program not distinguishing a plain task label ('t/T') and a task label with a task
  // digit ("t1"). Examples 1-3 work without distinguishing them because for example 1, section 3 attaches task_num 2 to
  // the existing subtl_vec element, for example 3, section 2 already attaches task_num to the subtl_vec element.
  // For simplicity, let's modify sections 1-3 to distinguish task types based on the task_num. No task_num is regarded as
  // an unclassified task, and a new element in subtl_vec is prepared. For example 1, when an unclassified task is anticipated
  // from the activity list but task 2 ("t2") is encountered, a new element with task_num 2 is pushed back to subtl_vec, and
  // no time is entered for the unclassified task in subtl_vec. But this still produces the correct result, because the
  // duration of the unclassified task is 0, so no time is added to record_act_time_vec or record_task_time_vec in main().
  // For example 3, an element prepared for t2 is not used, instead, a new unclassified task element is added to subtl_vec in
  // section 2.5 and 20m is put into the unclassified task. Then, section 4 below assign task_num 2 to the newly added
  // unclassified task. So examples 1-3 still work correctly (with an extra unused element in subtl_vec for examples 1 and 3).
  // This new way correctly classifies example 5 as well. But example 4 is still not correctly classified (but the incorrect
  // repetition of 35mins is removed, instead, a newly added unclassified task takes the total 35m, and later it got task_num
  // 2 in section 4 below, which is still incorrect).
  //
  // Obtain all "task \d" from t.activity_content with regex (see test_regex.cpp)
  string s{t.activity_content};
  vector<int> task_num_vec;
  smatch m;
  regex pat{R"(task (\d))"};
  // store all task digits in activity_content (e.g. '2' in "task 2") to task_num_vec
  while(regex_search(s, m, pat)){ // regex_search() catches only the first occurrence of the pattern
    task_num_vec.push_back(m[1].str()[0] - '0');
    // m[0] is the sub-match object representing the entire match ("task 1"), and m[1] has the group match ("1"),
    // .str() converts the submatch object into a string, and [0] takes the first character ('1') from "1".
    s = m.suffix().str();
    // smatch::suffix() returns a sub-match object representing the character sequence following the end of the
    // matched substring. Without this update, regex_search() keeps getting the same substring.

    // ref: https://cplusplus.com/reference/regex/regex_search/
  }
  // In the order they appear in t.activity_content, assign found task_nums
  int vec_i{0}; // record which index in task_num_vec to use
  if(task_num_vec.size()){
    // first, check the main activity
    if(t.a == activity_type::task && t.task_num == 0){
      t.task_num = task_num_vec[0];
      ++vec_i;
    }

    // scan all elements of t.subtl_vec and assign found task_nums
    for(int i=0; i < t.subtl_vec.size() && vec_i<task_num_vec.size(); ++i){
      if(t.subtl_vec[i].get_a() == activity_type::task && t.subtl_vec[i].task_num == 0){
	t.subtl_vec[i].task_num = task_num_vec[vec_i];
	++vec_i;
	//cout << "### sub activity is assigned " << t.subtl_vec[i].task_num << endl;
      }
    }
  }

  return is;
}

// constructor.
// Assume line is one line of timelines.
// e.g. line == "- t ~20:00 task 1 (making this program)"
Timeline::Timeline(string line)
  : Abst_Timeline{}
{
  istringstream iss{line};
  iss >> *this;			// read by using the overloaded operator >>
}

void Timeline::print_tl(){
  char buff[20];		// container to have date and time e.g. "2024-12-24 19:27"

  strftime(buff, sizeof(buff), "%F %H:%M", &end_t);
  // stores the time info in buff in C-style string
  
  cout << "Time stamp: " << buff << endl;
  // cout should be able to deal with C-style string (\0-ended string)

  cout << "Activity type: " << convert_a2s(a) << endl;

  cout << "Activity content: " << activity_content << endl;

  if(subtl_vec.size()>0){
    cout << "Sub-activities: " << endl;
    for(int i=0; i<subtl_vec.size(); ++i){
      cout << "\t";
      subtl_vec[i].print_subact();
    }
  }
}


int main(int argc, char** argv)
try{
  // test if I can instantiate Abst_Timeline (I should not be able to)
  //Abst_Timeline at{};
  // -> this line caused an error, as I expected
  
  if(argc < 2){
    throw invalid_argument("Error: you need to specify the text file name with timelines");
  }

  string fname{argv[1]};
  ifstream ifs{fname};
  if(!ifs)
    throw invalid_argument("Error: cannot open file " + fname);
  
  /* test Timeline's operator>>
  Timeline tl;
  string line;
  int c{1};			// line count
  while(getline(ifs, line)){
    cout << "line " << c << endl;
    istringstream iss{line};
    iss >> tl;
    tl.print_tl(); cout << endl;
    ++c;
  }
  */

  /* test Timeline(string s)
  string line;
  int c{1};			// line count
  while(getline(ifs, line)){
    cout << "line " << c << endl;
    Timeline tl{line};
    tl.print_tl(); cout << endl;
    ++c;
  }
  */

  string line;
  istringstream iss;
  
  // get the first line and read the date mm/dd/yyy
  getline(ifs, line);
  iss.str(line);
  //tm date;			// std::tm
  // for operator>>(istream& is, Timeline& t) to access the date info, I made date global
  iss >> get_time(&date, "%m/%d/%Y");
  // %m: 01-12. leading 0 is permitted but not required
  if(iss.fail()){
    cerr << "Error in reading the first line as a date\n";
    cerr << "Required format: mm/dd/yyyy, e.g. 9/15/2025" << endl;
    return 1;
  }
  date.tm_hour = 0;
  date.tm_min = 0;
  date.tm_sec = 0;
  // these pieces of info below date.tm_mday are not used, but to avoid any mistake when forwarding date by 1 day
  // below (if(e<b){...}), I set them to 0.
  
  vector<Timeline> tl_vec;
  int c{1}; // count the number of timelines
  vector<int> record_act_time_vec(int(activity_type::error)+1, 0); // initialize all elements to 0
  // To be able to specify the index by [int(activity_type)], I get an extra element +1
  vector<int> record_task_time_vec(1,0); // record task times of each task. [0] record unclassified task time (task_num==0)
  while(getline(ifs, line)){
    iss.clear();		// clear the previous flags
    iss.str(line);		// set a new line to istringstream
    // ref: https://stackoverflow.com/questions/2767298/c-repeatedly-using-istringstream

    tl_vec.push_back(Timeline()); // add an empty Timeline
    if(!(iss >> tl_vec[tl_vec.size()-1])){ // check if iss is in good() condition. if(istream) checks if istream is in good()
      cerr << "At " << c << "-th Timeline, an reading error happened\n";
      return 1;
    }
    //tl_vec.push_back(tl);
    // using the copy constructor of Timeline, copy the content of tl to a vector's element
      
    if(c > 1){			// calculate the number of minutes to pass from the last end time
      time_t b, e;
      tm *b_tm, *e_tm;		// used to create b and e of time_t
      b_tm = &(tl_vec[c-2].end_t);
      // Even though Abst_Timeline::get_tm() returns const struct tm&, b_tm (and e_tm) are non-const struct tm.
      // Is this fine?
      // -> yes, because b_tm is a copy of the referenced variable. Modifying b_tm doesn't change the original
      //    variable of tl_vec[c-2].get_tm().
      //tl_vec[c-2].get_tm().tm_year = 2000;
      // <- this line caused an error as expected, since this tries to modify the const variable.
      // So, modifying b_tm (e_tm) doesn't modify Timeline's end_t variable.
      // -> this is not good, because b_tm/e_tm in this main function is the most up-to-date one
      //    (correct year, month, and day are added). It's desirable for the Timeline objects to have
      //    the correct date info, as well as hour and minute.
      // -> So I changed b_tm/e_tm to a reference to end_t of the Timeline object, and update the year/month/day
      //    (also changed the return type of Abst_Timeline::get_tm() from const struct tm& to struct tm&).
      // -> If I do so, I can instead make end_t a public member and remove get_tm() function.

      e_tm = &(tl_vec[c-1].end_t);
      
      date = set_dates(date, b_tm, e_tm); // update date if necessary
      // when e_tm goes to the next day, update the global variable "date".
      
      b = mktime(b_tm);
      e = mktime(e_tm);
      
      double seconds = difftime(e, b);
      //cout << "### Duration [min] = " << seconds/60 << endl;

      // subtract sub-activities' durations from seconds and add them to record_act_time_vec
      // Also, add task time
      for(int i=0; i<tl_vec[c-1].get_subtl_size(); ++i){
	const Sub_Timeline subtl = tl_vec[c-1].get_subtl(i);
	seconds -= subtl.duration*60; // Sub_Timeline::duration is in minutes, so convert it to seconds
	if(seconds < 0){
	  cerr << "Error in subtracting sub-activity's duration from the main activity's duration." << endl;
	  cerr << c << "-th Timeline, ";
	  throw runtime_error("The main duration became negative");
	}
	record_act_time_vec[int(subtl.get_a())] += subtl.duration; // duration is in [minute], and store it in minutes
	if(subtl.get_a() == activity_type::task){
	  if(record_task_time_vec.size() < subtl.task_num+1)
	    record_task_time_vec.resize(subtl.task_num+1, 0);
	    // allocate a new memory of size (subtl.task_num+1), copy existing elements there, initialize new elements
	    // with 0 (2nd argument), delete an old memory.
	    // So the old elements remain in the resized array

	  record_task_time_vec[subtl.task_num] += subtl.duration;
	}
      }
      
      record_act_time_vec[int(tl_vec[c-1].get_a())] += seconds/60; // store in minutes
      if(tl_vec[c-1].get_a() == activity_type::task){
	// This .resize() is necessary for the main activity as well.
	// without this resize(), it tries to access unallocated address, which causes memory corruption error
	if(record_task_time_vec.size() < tl_vec[c-1].task_num+1)
	  record_task_time_vec.resize(tl_vec[c-1].task_num+1, 0);
	
	record_task_time_vec[tl_vec[c-1].task_num] += seconds/60;
      }
    }
    // for debug
    //cout << "### " << c << "-th Timeline:" << endl;
    //tl_vec[c-1].print_tl();
    //cout << endl;
    
    ++c;
  } // while(getline(ifs, line)){

  for(int i=1; i<=int(activity_type::rest); ++i){ // activity_type::not_set=1, so start with i=1
    switch(i){
    case int(activity_type::not_set):
      cout << "Total not_set time: " << record_act_time_vec[int(activity_type::not_set)] << " [mins]" << endl;
      break;
      
    case int(activity_type::task):
      cout << "Total Task time: " << record_act_time_vec[int(activity_type::task)] << " [mins]" << endl;
      cout << "\tUnclassified task time: " << record_task_time_vec[0] << " [mins]" << endl;
      for(int i=1; i<record_task_time_vec.size(); ++i){
	cout << "\tTask " << i << " time: " << record_task_time_vec[i] << " [mins]" << endl;
      }
      break;

    case int(activity_type::wasteful):
      cout << "Total wasteful activity time: " << record_act_time_vec[int(activity_type::wasteful)] << " [mins]" << endl;
      break;

    case int(activity_type::house_chore):
      cout << "Total house chore time: " << record_act_time_vec[int(activity_type::house_chore)] << " [mins]" << endl;
      break;

    case int(activity_type::social):
      cout << "Total social activity time: " << record_act_time_vec[int(activity_type::social)] << " [mins]" << endl;
      break;

    case int(activity_type::write_log):
      cout << "Total log writing time: " << record_act_time_vec[int(activity_type::write_log)] << " [mins]" << endl;
      break;

    case int(activity_type::miscellaneous):
      cout << "Total miscellaneous activity time: " << record_act_time_vec[int(activity_type::miscellaneous)] << " [mins]"  << endl;
      break;

    case int(activity_type::exercise):
      cout << "Total exercise time: " << record_act_time_vec[int(activity_type::exercise)] << " [mins]" << endl;
      break;

    case int(activity_type::travel):
      cout << "Total travel time: " << record_act_time_vec[int(activity_type::travel)] << " [mins]" << endl;
      break;

    case int(activity_type::rest):
      cout << "Total rest time: " << record_act_time_vec[int(activity_type::rest)] << " [mins]" << endl;
      break;

    case int(activity_type::pastime):
      cout << "Total rest time: " << record_act_time_vec[int(activity_type::pastime)] << " [mins]" << endl;
      break;
    }
  }
  
  return 0;
 }
 catch(exception& e){
   cerr << e.what() << endl;
   return 1;
 }
 catch(...){
   cerr << "Error: Unknown exception is caught\n";
   return 1;
 }
