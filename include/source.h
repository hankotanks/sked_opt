#ifndef SOURCE_H__
#define SOURCE_H__

typedef struct {
    char name[8];
    double raan, decl, epoch;
} Source;

void
Source_dump(const Source* const src);

#endif // SOURCE_H__
