#include "time_sys.h"

#include <math.h>
#include <time.h>

#include "hh.h"

#include "xml.h"
#include "xml_util.h"

size_t
months_count_days(size_t yrs, enum month mon) {
    static const size_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if(mon == FEB && ((yrs % 4 == 0 && (yrs % 100 != 0 || yrs % 400 == 0)))) return 29;
    return days[mon - 1];
}

double
DateTime_to_mjd(DateTime dt) {
    int yrs = (int) dt.yrs - (int) (dt.mon == JAN || dt.mon == FEB);
    int mon = (int) dt.mon + ((dt.mon == JAN || dt.mon == FEB) ? 12 : 0) + 1;
    int day_int = (int) dt.day;
    int A = yrs / 100;
    int B = 2 - A + (A / 4);
    double JD = floor(365.25 * (yrs + 4716)) + floor(30.6001 * mon) + day_int + (double) B - 1524.5;
    double day_frac = ((double) dt.hrs + ((double) dt.min + (double) dt.sec / 60.0) / 60.0) / 24.0;
    return (JD + day_frac) - 2400000.5;
}

DateTime
DateTime_from_mjd(double mjd) {
    DateTime dt;
    double jd = (mjd + 2400000.5);
    double jd_int = floor(jd + 0.5);
    double jd_frac = jd + 0.5 - jd_int;
    size_t L = (size_t) jd_int + 68569;
    size_t N = (4 * L) / 146097;
    L = L - (146097 * N + 3) / 4;
    size_t I = (4000 * (L + 1)) / 1461001;
    L = L - (1461 * I) / 4 + 31;
    size_t J = (80 * L) / 2447;
    size_t K = L - (2447 * J) / 80;
    L = J / 11;
    J = J + 2 - 12 * L;
    I = 100 * (N - 49) + I + L;
    dt.yrs = I;
    dt.mon = (enum month) J;
    dt.day = K;
    double jd_min = jd_frac * 1440.0;
    dt.hrs = (size_t) (jd_min / 60.0);
    dt.min = ((size_t) jd_min) % 60;
    dt.sec = (jd_min - ((double) dt.hrs * 60.0 + (double) dt.min)) * 60.0;
    return dt;
}

double
DateTime_to_gmst(DateTime dt) {
    double jd = DateTime_to_mjd(dt) + 2400000.5;
    // Adapted from https://www.mathworks.com/matlabcentral/fileexchange/28176-julian-date-to-greenwich-mean-sidereal-time
    // by Darin Koblick
    double jd0, hrs, min, max;
    min = floor(jd) - 0.5;
    max = floor(jd) + 0.5;
    jd0 = (jd > max) ? max : min;
    hrs = \
        (6.697374558) + \
        (0.06570982441908 * (jd0 - 2451545.0)) + \
        (1.00273790935 * (jd - jd0) * 24.0) + \
        (0.000026 * pow((jd - 2451545.0) / 36525.0, 2.0));
    return fmod(hrs, 24.0) * 15.0;
}

const char* months[12] = {
#define X(name_, val_) #name_,
    TIME_SYS_H__MONTHS
#undef X
};

static TimeSys TIME_SYS_H__time_sys; TimeSys* time_sys = &TIME_SYS_H__time_sys;

void
time_sys_init(void) {
    time_sys->start = DateTime_from_mjd(((double) time(NULL)) / 86400.0 + 2440587.5  - 2400000.5);
    time_sys->duration = 86400;
    time_sys->scan_length = 30;
}

const char*
time_sys_text(unsigned int seconds) {
    DateTime dt = DateTime_from_mjd(DateTime_to_mjd(time_sys->start) + ((double) seconds / 86400.0));
    static char buf[16];
    snprintf(buf, 5, "%04zu", dt.yrs);
    snprintf(buf + 4, 4, "%s", months[dt.mon - 1]);
    snprintf(buf + 7, 3, "%02zu", dt.day);
    buf[9] = ' ';
    snprintf(buf + 10, 3, "%02zu", dt.hrs);
    buf[12] = ':';
    snprintf(buf + 13, 3, "%02zu", dt.min);
    return buf;
}

const char*
time_sys_file(void) {
    static char buf[19];
    snprintf(buf, 5, "%04zu", time_sys->start.yrs);
    snprintf(buf + 4, 4, "%s", months[time_sys->start.mon - 1]);
    snprintf(buf + 7, 3, "%02zu", time_sys->start.day);
    buf[9] = '_';
    snprintf(buf + 10, 3, "%02zu", time_sys->start.hrs);
    snprintf(buf + 12, 3, "%02zu", time_sys->start.min);
    strcpy(buf + 14, ".skd");
    return buf;
}

bool
time_sys_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    if(general == NULL) return false;
    // parse schedule start
    struct xml_node* startTime = xml_node_find(general, "startTime");
    if(startTime == NULL) return false;
    char* start = malloc(xml_node_content(startTime)->length + 1);
    if(start == NULL) return false;
    strncpy(start, (const char*) xml_node_content(startTime)->buffer, xml_node_content(startTime)->length);
    start[xml_node_content(startTime)->length] = '\0';
    int mon;
    int result = sscanf(start, " %04zu.%02d.%02zu %02zu:%02zu:%02lf",
        &time_sys->start.yrs,
        &mon,
        &time_sys->start.day,
        &time_sys->start.hrs,
        &time_sys->start.min,
        &time_sys->start.sec);
    free(start);
    if(result != 6) return false;
    time_sys->start.mon = (enum month) mon;
    // parse schedule end
    struct xml_node* endTime = xml_node_find(general, "endTime");
    if(endTime == NULL) return false;
    char* end = malloc(xml_node_content(endTime)->length + 1);
    if(end == NULL) return false;
    strncpy(end, (const char*) xml_node_content(endTime)->buffer, xml_node_content(endTime)->length);
    end[xml_node_content(endTime)->length] = '\0';
    // write to another DateTime
    DateTime final;
    result = sscanf(end, " %04zu.%02d.%02zu %02zu:%02zu:%02lf",
        &final.yrs, &mon, &final.day,
        &final.hrs, &final.min, &final.sec);
    free(end);
    if(result != 6) return false;
    final.mon = (enum month) mon;
    // calculate duration
    time_sys->duration = (unsigned int) ((DateTime_to_mjd(final) - DateTime_to_mjd(time_sys->start)) * 86400.0);
    // parse scan length
    struct xml_node* station = xml_node_find(root, "station");
    if(station == NULL) return false;
    struct xml_node* parameters = xml_node_find(station, "parameters");
    if(parameters == NULL) return false;
    struct xml_node* parameter_default = xml_node_find_with_attr(parameters, "parameter", "name", "default");
    if(parameter_default == NULL) return false;
    struct xml_node* minScan = xml_node_find(parameter_default, "minScan");
    if(minScan == NULL) return false;
    char* scan_length = malloc(xml_node_content(minScan)->length + 1);
    if(scan_length == NULL) return false;
    strncpy(scan_length, (const char*) xml_node_content(minScan)->buffer, xml_node_content(minScan)->length);
    scan_length[xml_node_content(minScan)->length] = '\0';
    result = sscanf(scan_length, " %u ", &time_sys->scan_length);
    free(scan_length);
    if(result != 1) return false;
    return true;
}
