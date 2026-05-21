/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     xtime.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license. 

********************************************************************************/

#include "herss.h"

Xtime::Xtime() {}
Xtime::~Xtime() {}

//---------------------------------------------------------------------------------------
void Xtime::printDate() {
    printf("%4d%02d%02d \n", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);
}
//---------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------
// Function returning a string with the date in the format YYYYMMDDHH.
string Xtime::returnDateString() {
    char str_buffer[15];
    strftime(str_buffer , 15, "%Y%m%d%H", &my_tm);
    return string(str_buffer);
}
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
void Xtime::getDateString(char *str_buffer) {

    // sprintf(str_buffer, "%4d%02d%02d", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);
    // printf("%4d %02d %02d %02d ", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, my_tm.tm_hour );
    strftime(str_buffer , 15, "%Y%m%d%H", &my_tm);

}
//---------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------
void Xtime::getNowString(std::string &s) {
    time_t now;
    char buffer[100];
    struct tm *t;
    time(&now);
    t = localtime(&now);
    // sprintf(buffer, "%4d%02d%02d%02d%02d%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    sprintf(buffer, "%d %d %d %02d:%02d:%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    s = buffer;
}
//---------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------
void Xtime::setDate(int yyyy, int mm, int day, int hour, int min, int sec) {
    // cout << "In function SetDate" << endl;
    my_tm.tm_sec   = sec;            // 0 to 59 (or 60 for occasional rare leap-seconds)
    my_tm.tm_min   = min;            // 0 to 59
    my_tm.tm_hour  = hour;           // 0 to 23
    my_tm.tm_mday  = day;            // 1 to 31
    my_tm.tm_mon   = mm-1;           // 0 to 11, stupidly 0=January, 11=December
    my_tm.tm_year  = yyyy-1900;      // year-1900, so 79 means 1979, 103 means 2003
    //           int tm_wday;         // 0 to 6, 0=Sunday, 1=Monday, ..., 6=Saturday
    //           int tm_yday;         // 0 to 365, 0=1st January
    my_tm.tm_isdst = 0;              // 0 to 1, 1=DST is in effect, 0=it isn't. WE ARE ALWAYS USING NORMAL TIME. (NORWEGIAN WINTER TIME.)
    // Now we calculate EPOCH seconds.
    epoch = timegm(&my_tm);
    // We now go back to tm structure so we can get weeknr and day in year.
    gmtime_r(&epoch, &my_tm);
}

int Xtime::changeDate(int dt) {
    epoch += dt;
    gmtime_r(&epoch, &my_tm);
    return 0;
}


int Xtime::getYear()      { return (my_tm.tm_year+1900); }
int Xtime::getMonth()     { return (my_tm.tm_mon+1);     }
int Xtime::getDay()       { return  my_tm.tm_mday;       }
int Xtime::getHour()      { return  my_tm.tm_hour;       }
int Xtime::getMin()       { return  my_tm.tm_min;        }
int Xtime::getSec()       { return  my_tm.tm_sec;        }
int Xtime::getJulianDay() { return  my_tm.tm_yday;       }
time_t Xtime::getEpoch()  { return  epoch;               }

int Xtime::dateDiff(Xtime *theDate, int dt) {
    return ( (theDate->getEpoch() - epoch) / dt);
}

void Xtime::print(FILE *f, char *fmt) {
    char buf[15];
    strftime(buf, 15, "%Y%m%d%H%M%S", &my_tm);
    fprintf(f, fmt, buf);
}


////////////////////////////////////////////////////////////////////////////////////////////////
bool Xtime::setDate(string str_date) {
	
    int year,month,day,hour;
    char c = '-';

    // Supported date formats
    // yyyy-mm-dd
    // yyyy-mm-dd-hh
    // yyyymmdd
    // yyyymmddhh


    // yyyy-mm-dd-hh
    if(str_date.length() == 13 &&  str_date.find(c) == 4 ) {
        year  = atoi(str_date.substr(0, 4).c_str());
        month = atoi(str_date.substr(5, 2).c_str());
        day   = atoi(str_date.substr(8, 2).c_str());
        hour  = atoi(str_date.substr(11,2).c_str());
        if(isValid(year, month, day, hour, 0,0)) {
            setDate(year, month, day, hour,0,0);
        } else {
            return false;
        }
        return true;
    }


    //  yyyy-mm-dd
    if(str_date.length() == 10 &&  str_date.find(c) == 4 ) {
        year  = atoi(str_date.substr(0, 4).c_str());
        month = atoi(str_date.substr(5, 2).c_str());
        day   = atoi(str_date.substr(8, 2).c_str());
        hour = 0;
        if(isValid(year, month, day, hour, 0,0)) {
            setDate(year, month, day, hour,0,0);
        } else {
            return false;
        }
        return true;
    }

    //  yyyymmdd
    if(str_date.length() == 8 ) {
		year   = atoi( str_date.substr(0,4).c_str() );
		month  = atoi( str_date.substr(4,2).c_str() );
		day    = atoi( str_date.substr(6,2).c_str() );
        hour   = 0;
        if(isValid(year, month, day, hour, 0,0)) {
            setDate(year, month, day, hour,0,0);
        } else {
            return false;
        }
        return true;
    }

    // yyyymmddhh
    if(str_date.length() == 10 ) {
		year   = atoi( str_date.substr(0,4).c_str() );
		month  = atoi( str_date.substr(4,2).c_str() );
		day    = atoi( str_date.substr(6,2).c_str() );
        hour   = atoi( str_date.substr(8,2).c_str() );
        if(isValid(year, month, day, hour, 0,0)) {
            setDate(year, month, day, hour,0,0);
        } else {
            return false;
        }
        return true;
    }

	LOG_WARN(std::string("ERROR Something wrong with str_date= ") + str_date);
    return false;

}
////////////////////////////////////////////////////////////////////////////////////////////////
bool Xtime::isValid(std::string &date) {return false;}


bool Xtime::isValid(int year, int month, int day, int hour, int min, int sec) {
    if (year < 1900 || year > 2038) {
        printf("Something wrong with date    Year %d not valid\n", year);
        return false;
    }
    if (month < 1 || month > 12) {
        printf("Something wrong with date    Month %d not valid\n", month);
        return false;
    }
    if (day < 1 || day > 31) {
        printf("Something wrong with date    Days %d not valid\n", day);
        return false;
    }
    if (month == 2 &&  day > (isLeapYear(year) + 28)) {
        printf("Something wrong with date   Days %d in month %d for year %d not valid\n", day, month, year);
        return false;
    }
    if ( (month == 4 || month == 6 || month == 9 || month == 11) && (day > 30)) {
        printf("Something wrong with date   Days %d in month %d not valid\n", day, month);
        return false;
    }
    if (hour < 0 || hour > 23) {
        printf("Something wrong with date    Hour %d not valid\n", hour);
        return false;
    }
    if (min < 0 || min > 59) {
        printf("Something wrong with date   Minute %d not valid\n", min);
        return false;
    }
    if (sec < 0 || sec > 61) {
        printf("Something wrong with date   Seconds %d not valid\n", sec);
        return false;
    }
    return true;
}



int Xtime::isLeapYear(int year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
       return 1;
   else
       return 0;
}

string Xtime::currentDateTime() { 
    time_t     now = time(0); 
    struct tm  tstruct; 
    char       buf[80]; 
    tstruct = *localtime(&now); 
    strftime(buf, sizeof(buf), "%Y-%m-%d:%X", &tstruct); 
    return buf; 
} 
 



