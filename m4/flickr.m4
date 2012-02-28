AC_DEFUN([FLICKR_SERVICE_WITH_GOA],
[
AC_MSG_CHECKING([whether to use GOA credentials in Flickr service])
AC_ARG_WITH(
  [flickr-goa], 
  [AS_HELP_STRING([--with-flickr-goa], [build Flickr with support for GNOME Online Accounts])],
  [AS_IF(
    [test "x$with_flickr_goa" = xyes],
    [
    AC_DEFINE([FLICKR_WITH_GOA], [1], [Flickr with support for GNOME Online Accounts])
    AC_MSG_RESULT([yes])
    ],
    [AC_MSG_RESULT([no])]
  )],
  [AC_MSG_RESULT([no])]
)
])
