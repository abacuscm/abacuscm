#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifndef PREFIX
#define PREFIX	"/usr"
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR	PREFIX "/etc"
#endif

#ifndef SYSLIBDIR
#define SYSLIBDIR	PREFIX	"/lib"
#endif

#ifndef MODDIR
#define MODDIR	SYSLIBDIR "/abacus"
#endif

#endif
