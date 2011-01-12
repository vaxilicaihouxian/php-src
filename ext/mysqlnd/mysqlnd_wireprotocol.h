/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Georg Richter <georg@mysql.com>                             |
  |          Andrey Hristov <andrey@mysql.com>                           |
  |          Ulf Wendel <uwendel@mysql.com>                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef MYSQLND_WIREPROTOCOL_H
#define MYSQLND_WIREPROTOCOL_H

#include "mysqlnd_net.h"

#define MYSQLND_HEADER_SIZE 4
#define COMPRESSED_HEADER_SIZE 3

#define MYSQLND_NULL_LENGTH	(unsigned long) ~0

/* Used in mysqlnd_debug.c */
PHPAPI extern const char mysqlnd_read_header_name[];
PHPAPI extern const char mysqlnd_read_body_name[];


/* Packet handling */
#define PACKET_WRITE(packet, conn)	((packet)->header.m->write_to_net((packet), (conn) TSRMLS_CC))
#define PACKET_READ(packet, conn)	((packet)->header.m->read_from_net((packet), (conn) TSRMLS_CC))
#define PACKET_FREE(packet) \
	do { \
		DBG_INF_FMT("PACKET_FREE(%p)", packet); \
		if ((packet)) { \
			((packet)->header.m->free_mem((packet), FALSE TSRMLS_CC)); \
		} \
	} while (0);

PHPAPI extern const char * const mysqlnd_command_to_text[COM_END];

/* Low-level extraction functionality */
typedef struct st_mysqlnd_packet_methods {
	size_t				struct_size;
	enum_func_status	(*read_from_net)(void *packet, MYSQLND *conn TSRMLS_DC);
	size_t				(*write_to_net)(void *packet, MYSQLND *conn TSRMLS_DC);
	void				(*free_mem)(void *packet, zend_bool stack_allocation TSRMLS_DC);
} mysqlnd_packet_methods;


typedef struct st_mysqlnd_packet_header {
	size_t		size;
	mysqlnd_packet_methods *m;
	zend_uchar	packet_no;
	zend_bool	persistent;
} MYSQLND_PACKET_HEADER;

/* Server greets the client */
typedef struct st_mysqlnd_packet_greet {
	MYSQLND_PACKET_HEADER		header;
	uint8_t		protocol_version;
	char		*server_version;
	uint32_t	thread_id;
	zend_uchar	intern_scramble_buf[SCRAMBLE_LENGTH];
	zend_uchar	* scramble_buf;
	size_t		scramble_buf_len;
	/* 1 byte pad */
	uint32_t	server_capabilities;
	uint8_t		charset_no;
	uint16_t	server_status;
	/* 13 byte pad, in 5.5 first 2 bytes are more capabilities followed by 1 byte scramble_length */
	zend_bool	pre41;
	/* If error packet, we use these */
	char 		error[MYSQLND_ERRMSG_SIZE+1];
	char 		sqlstate[MYSQLND_SQLSTATE_LENGTH + 1];
	unsigned int 	error_no;
	char		*auth_protocol;
} MYSQLND_PACKET_GREET;


/* Client authenticates */
typedef struct st_mysqlnd_packet_auth {
	MYSQLND_PACKET_HEADER		header;
	uint32_t	client_flags;
	uint32_t	max_packet_size;
	uint8_t	charset_no;
	/* 23 byte pad */
	const char	*user;
	/* 8 byte scramble */
	const char	*db;
	/* 12 byte scramble */

	/* Here the packet ends. This is user supplied data */
	const char	*password;
	/* +1 for \0 because of scramble() */
	unsigned char	*server_scramble_buf;
	size_t			server_scramble_buf_len;
	size_t			db_len;
	zend_bool		send_auth_data;
	zend_bool		is_change_user_packet;
	zend_bool		silent;
} MYSQLND_PACKET_AUTH;

/* OK packet */
typedef struct st_mysqlnd_packet_ok {
	MYSQLND_PACKET_HEADER		header;
	uint8_t		field_count; /* always 0x0 */
	uint64_t	affected_rows;
	uint64_t	last_insert_id;
	uint16_t	server_status;
	uint16_t	warning_count;
	char		*message;
	size_t		message_len;
	/* If error packet, we use these */
	char 		error[MYSQLND_ERRMSG_SIZE+1];
	char 		sqlstate[MYSQLND_SQLSTATE_LENGTH + 1];
	unsigned int 	error_no;
} MYSQLND_PACKET_OK;


/* Command packet */
typedef struct st_mysqlnd_packet_command {
	MYSQLND_PACKET_HEADER			header;
	enum php_mysqlnd_server_command	command;
	const char						*argument;
	size_t							arg_len;
} MYSQLND_PACKET_COMMAND;


/* EOF packet */
typedef struct st_mysqlnd_packet_eof {
	MYSQLND_PACKET_HEADER		header;
	uint8_t		field_count; /* 0xFE */
	uint16_t	warning_count;
	uint16_t	server_status;
	/* If error packet, we use these */
	char 		error[MYSQLND_ERRMSG_SIZE+1];
	char 		sqlstate[MYSQLND_SQLSTATE_LENGTH + 1];
	unsigned int 	error_no;
} MYSQLND_PACKET_EOF;
/* EOF packet */


/* Result Set header*/
typedef struct st_mysqlnd_packet_rset_header {
	MYSQLND_PACKET_HEADER		header;
	/*
	  0x00 => ok
	  ~0   => LOAD DATA LOCAL
	  error_no != 0 => error
	  others => result set -> Read res_field packets up to field_count
	*/
	unsigned long		field_count;
	/*
	  These are filled if no SELECT query. For SELECT warning_count
	  and server status are in the last row packet, the EOF packet.
	*/
	uint16_t	warning_count;
	uint16_t	server_status;
	uint64_t	affected_rows;
	uint64_t	last_insert_id;
	/* This is for both LOAD DATA or info, when no result set */
	char		*info_or_local_file;
	size_t		info_or_local_file_len;
	/* If error packet, we use these */
	MYSQLND_ERROR_INFO	error_info;
} MYSQLND_PACKET_RSET_HEADER;


/* Result set field packet */
typedef struct st_mysqlnd_packet_res_field {
	MYSQLND_PACKET_HEADER	header;
	MYSQLND_FIELD			*metadata;
	/* For table definitions, empty for result sets */
	zend_bool				skip_parsing;
	zend_bool				stupid_list_fields_eof;
	zend_bool				persistent_alloc;

	MYSQLND_ERROR_INFO		error_info;
} MYSQLND_PACKET_RES_FIELD;


/* Row packet */
typedef struct st_mysqlnd_packet_row {
	MYSQLND_PACKET_HEADER	header;
	zval		**fields;
	uint32_t	field_count;
	zend_bool	eof;
	/*
	  These are, of course, only for SELECT in the EOF packet,
	  which is detected by this packet
	*/
	uint16_t	warning_count;
	uint16_t	server_status;

	struct st_mysqlnd_memory_pool_chunk	*row_buffer;
	MYSQLND_MEMORY_POOL * result_set_memory_pool;

	zend_bool		skip_extraction;
	zend_bool		binary_protocol;
	zend_bool		persistent_alloc;
	MYSQLND_FIELD	*fields_metadata;
	/* We need this to alloc bigger bufs in non-PS mode */
	unsigned int	bit_fields_count;
	size_t			bit_fields_total_len; /* trailing \0 not counted */

	/* If error packet, we use these */
	MYSQLND_ERROR_INFO	error_info;
} MYSQLND_PACKET_ROW;


/* Statistics packet */
typedef struct st_mysqlnd_packet_stats {
	MYSQLND_PACKET_HEADER	header;
	char *message;
	/* message_len is not part of the packet*/
	size_t message_len;
} MYSQLND_PACKET_STATS;


/* COM_PREPARE response packet */
typedef struct st_mysqlnd_packet_prepare_response {
	MYSQLND_PACKET_HEADER	header;
	/* also known as field_count 0x00=OK , 0xFF=error */
	unsigned char	error_code;
	unsigned long	stmt_id;
	unsigned int	field_count;
	unsigned int	param_count;
	unsigned int	warning_count;

	/* present in case of error */
	MYSQLND_ERROR_INFO	error_info;
} MYSQLND_PACKET_PREPARE_RESPONSE;


/* Statistics packet */
typedef struct st_mysqlnd_packet_chg_user_resp {
	MYSQLND_PACKET_HEADER	header;
	uint32_t			field_count;

	/* message_len is not part of the packet*/
	uint16_t			server_capabilities;
	/* If error packet, we use these */
	MYSQLND_ERROR_INFO	error_info;
	zend_bool			server_asked_323_auth;
} MYSQLND_PACKET_CHG_USER_RESPONSE;


PHPAPI void php_mysqlnd_scramble(zend_uchar * const buffer, const zend_uchar * const scramble, const zend_uchar * const pass);

unsigned long	php_mysqlnd_net_field_length(zend_uchar **packet);
zend_uchar *	php_mysqlnd_net_store_length(zend_uchar *packet, uint64_t length);

PHPAPI const extern char * const mysqlnd_empty_string;


enum_func_status php_mysqlnd_rowp_read_binary_protocol(MYSQLND_MEMORY_POOL_CHUNK * row_buffer, zval ** fields,
										 unsigned int field_count, MYSQLND_FIELD *fields_metadata,
										 zend_bool persistent,
										 zend_bool as_unicode, zend_bool as_int_or_float,
										 MYSQLND_STATS * stats TSRMLS_DC);


enum_func_status php_mysqlnd_rowp_read_text_protocol(MYSQLND_MEMORY_POOL_CHUNK * row_buffer, zval ** fields,
										 unsigned int field_count, MYSQLND_FIELD *fields_metadata,
										 zend_bool persistent,
										 zend_bool as_unicode, zend_bool as_int_or_float,
										 MYSQLND_STATS * stats TSRMLS_DC);


PHPAPI MYSQLND_PROTOCOL * mysqlnd_protocol_init(zend_bool persistent TSRMLS_DC);
PHPAPI void mysqlnd_protocol_free(MYSQLND_PROTOCOL * const protocol TSRMLS_DC);
PHPAPI struct st_mysqlnd_protocol_methods * mysqlnd_protocol_get_methods();

#endif /* MYSQLND_WIREPROTOCOL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
