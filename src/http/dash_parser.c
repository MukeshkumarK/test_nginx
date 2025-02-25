#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include<linux/limits.h>

#include <libxml/parser.h>

char * get_dash_playlist(ngx_http_request_t *r, char *path) {

	DIR *d;
	struct dirent *dir;

	char *directory = get_dir(r, path);

	char *playlist_path = (char*) malloc(PATH_MAX);
	memset(playlist_path, '\0', PATH_MAX);

	int playlist_found = 0;

	d = opendir(directory);
	if (d) {
		while ((dir = readdir(d)) != NULL) {

			char *filename = dir->d_name;

			if (strstr(filename, "mpd") != NULL) {

				strcpy(playlist_path, directory);
				strcat(playlist_path, filename);
				playlist_found = 1;
				break;
			}
		}
		closedir(d);
	}

	if (playlist_found == 1)
		return playlist_path;
	return NULL;

}

int is_dash_playlist_found(ngx_http_request_t *r, char *path) {

	char *playlist_path = get_dash_playlist(r, path);

	if (playlist_path != NULL)
		return 1;
	return 0;

}

xmlNode* get_element_child(ngx_http_request_t *r, xmlNode* root,
		int child_count) {

	xmlNode* first_child = root->children;
	xmlNode* node;
	xmlNode* rv = NULL;
	int cnt = 1;

	for (node = first_child; node; node = node->next) {

		if (node->type == XML_ELEMENT_NODE) {

			if (cnt == child_count) {
				rv = node;
				break;
			}

			cnt++;

		}
	}

	return rv;

}
xmlNode* get_segment_template(ngx_http_request_t *r, xmlNode* representation) {

	xmlNode* node = get_element_child(r, representation, 1);
	if (strcmp((const char *) node->name, "SegmentTemplate") == 0) {
		return node;
	} else
		return get_element_child(r, representation, 2);

}

char* get_media_format(ngx_http_request_t *r, xmlNode *adaptation_set) {

	ngx_log_t *log;
	log = r->connection->log;

	xmlNode* representation = get_element_child(r, adaptation_set, 1);

	char *rep_id = (char *) xmlGetProp(representation, (const xmlChar *) "id");

	xmlNode* segment_template = get_segment_template(r, representation);

	char* media = (char*) xmlGetProp(segment_template,
			(const xmlChar *) "media");

	char *ptr = strchr(media, '$');
	int len = strlen(media) - (ptr - media);
	memset(ptr, '\0', len);

	len = sizeof(media) + strlen(rep_id) + 10;
	char* media_format_result = (char*) malloc(len);
	memset(media_format_result, '\0', len);

	strcpy(media_format_result, media);
	strcat(media_format_result, rep_id);

	return media_format_result;
}

char* get_codecs(ngx_http_request_t *r, xmlNode *adaptation_set) {
	xmlNode* representation = get_element_child(r, adaptation_set, 1);
	xmlChar *codecs = NULL;
	codecs = xmlGetProp(representation, (const xmlChar *) "codecs");
	return (char*) codecs;

}

char *get_file_format(ngx_http_request_t *r, char *input_codecs, xmlNode *root) {

	xmlNode *first_child, *node;
	first_child = root->children;
	char *file_format = NULL;

	ngx_log_t *log;
	log = r->connection->log;

	for (node = first_child; node; node = node->next) {

		if (strcmp((const char *) node->name, (const char *) "Period") == 0) {

			xmlNode* first_child1 = node->children;
			xmlNode* node1;
			for (node1 = first_child1; node1; node1 = node1->next) {

				if (node1->type == XML_ELEMENT_NODE) {
					char* codecs = get_codecs(r, node1);
					char *m_ptr = strstr(codecs, input_codecs);
					if (strlen(input_codecs) == 0) {
						if (strlen(codecs) == 0) {
							file_format = get_media_format(r, node1);
							break;

						}

					}

					else if (m_ptr) {
						file_format = get_media_format(r, node1);
						break;

					}

				}
			}

			break;
		}

	}

	return file_format;

}

char* get_audio_file_format(ngx_http_request_t *r, xmlNode *root) {

	char *audio_file_format = get_file_format(r, "mp4a.40.2", root);
	return audio_file_format;

}

char* get_video_file_format(ngx_http_request_t *r, xmlNode *root) {
	char *video_file_format = get_file_format(r, "avc1.6400", root);
	return video_file_format;
}

char* get_subtitle_file_format(ngx_http_request_t *r, xmlNode *root) {
	char *subtitle_file_format = get_file_format(r, "", root);
	return subtitle_file_format;

}
enum stream_file_type sft;

enum stream_file_type* dash_get_file_type(ngx_http_request_t *r, char* path) {

	xmlDoc *document;
	xmlNode *root;
	char *playlist_path = get_dash_playlist(r, path);
	ngx_log_t *log;

	log = r->connection->log;

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
			"ngx_http_static_handler: path-  %s\n", path);

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
			"ngx_http_static_handler: playlist_path-  %s\n", playlist_path);

	document = xmlReadFile(playlist_path, NULL, 0);
	root = xmlDocGetRootElement(document);

	char *audio_file_format = get_audio_file_format(r, root);
	char *video_file_format = get_video_file_format(r, root);
	char *subtitle_file_format = get_subtitle_file_format(r, root);

	if ((audio_file_format != NULL)
			&& (strstr(path, audio_file_format) != NULL)) {

		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
				"ngx_http_static_handler: audio_file_format-  %s\n",
				audio_file_format);
		sft = AUDIO;
	} else if ((video_file_format != NULL)
			&& (strstr(path, video_file_format) != NULL)) {
		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
				"ngx_http_static_handler: video_file_format-  %s\n",
				video_file_format);

		sft = VIDEO;
	} else if ((subtitle_file_format != NULL)
			&& (strstr(path, subtitle_file_format) != NULL)) {

		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
				"ngx_http_static_handler: subtitle_file_format-  %s\n",
				subtitle_file_format);
		sft = SUBTITLE;
	} else {
		sft = UNDEFINED;
	}

	return (enum stream_file_type*) &sft;

}

