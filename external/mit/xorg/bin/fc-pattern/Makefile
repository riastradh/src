#	$NetBSD: Makefile,v 1.3 2018/07/04 12:25:49 maya Exp $

.include <bsd.own.mk>

PROG=	fc-pattern

CPPFLAGS+=	-I${X11SRCDIR.fontconfig} -I. -I${DESTDIR}${X11INCDIR}/freetype2
CPPFLAGS+=	-DHAVE_GETOPT_LONG=1

LDADD+=	-lfontconfig -lfreetype -lexpat
DPADD+=	${LIBFONTCONFIG} ${LIBFREETYPE} ${LIBEXPAT}

.PATH:	${X11SRCDIR.fontconfig}/${PROG}

FCARCH_DEPFILE=	fc-cat.c
.include "../../lib/fontconfig/src/Makefile.fcarch"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
