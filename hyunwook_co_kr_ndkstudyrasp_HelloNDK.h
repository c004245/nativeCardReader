#ifndef BE_APP_INFO_H
#define BE_APP_INFO_H

static const char * const PACKAGE = "hyunwook/co/kr/ndkstudyrasp/reader";

static const char * const CALLBACK_SIGNATURE = "([B)V";
static const char * const CALLBACK_FC = "readCompleted";

static const char * const INIT_SIGNATURE = "([BI)I";
static const char * const INIT_FC = "initCardReader";

static const char * const START_SIGNATURE = "()I";
static const char * const START_FC = "runCardReader";

static const char * const ENDED_SIGNATURE = "()I";
static const char * const ENDED_FC = "terminateCardReader";

#endif

