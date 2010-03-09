#include "c.h"

extern Interface gvmtIR;
extern Interface svgIR;
extern Interface nullIR;

Binding bindings[] = {
    "gvmt", &gvmtIR,
    "svg", &svgIR,
    "null", &nullIR,
	NULL, NULL
};

