
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include<linux/limits.h>

typedef struct node {
	void* data;
	struct node* next;
} Node;

typedef struct list {
	int size;
	Node* head;
} List;

List* create_list() {
	List* new_list = (List*) malloc(sizeof(List));
	new_list->size = 0;
	new_list->head = NULL;
	return new_list;
}

void add_to_list(List* list, void* data) {
	Node* new_node = (Node*) malloc(sizeof(Node));
	new_node->data = data;
	new_node->next = list->head;
	list->head = new_node;
	list->size++;
}

void* remove_from_list(List* list) {
	if (list->size == 0) {
		return NULL;
	}
	Node* node_to_remove = list->head;
	void* data = node_to_remove->data;
	list->head = node_to_remove->next;
	free(node_to_remove);
	list->size--;
	return data;
}

void free_list(List* list) {
	Node* current_node = list->head;
	while (current_node != NULL) {
		Node* next_node = current_node->next;
		free(current_node);
		current_node = next_node;
	}
	free(list);
}

char *get_hls_master_playlist(ngx_http_request_t *r, char *path) {

	DIR *d;
	struct dirent *dir;

	char *directory = get_dir(r, path);

	char *master_playlist_path = (char*) malloc(PATH_MAX);
	memset(master_playlist_path, '\0', PATH_MAX);

	int master_playlist_found = 0;

	d = opendir(directory);
	if (d) {
		while ((master_playlist_found == 0) && (dir = readdir(d)) != NULL) {

			char *filename = dir->d_name;

			if (strstr(filename, "m3u8") != NULL) {

				char playlist_file[PATH_MAX];
				memset(playlist_file, '\0', PATH_MAX);

				strcpy(playlist_file, directory);
				strcat(playlist_file, filename);
				FILE* file = fopen(playlist_file, "r");

				if (file != NULL) {
					char line[1024];

					while (fgets(line, sizeof(line), file)) {
						if ((strstr(line, "#EXT-X-MEDIA:") != NULL)
								|| (strstr(line, "#EXT-X-STREAM-INF") != NULL)) {
							strcpy(master_playlist_path, playlist_file);
							master_playlist_found = 1;
							break;
						}
					}

					fclose(file);
				}

			}

		}
		closedir(d);
	}

	if (master_playlist_found == 1)
		return master_playlist_path;
	return NULL;
}

char *get_audio_playlist(ngx_http_request_t *r, char *master_playlist_path) {
	int audio_playlist_found = 0;
	char *audio_playlist_path = (char*) malloc(PATH_MAX);
	memset(audio_playlist_path, '\0', PATH_MAX);

	char *directory = get_dir(r, master_playlist_path);
	strcpy(audio_playlist_path, directory);

	FILE* file = fopen(master_playlist_path, "r");

	if (file != NULL) {
		char line[1024];
		memset(line, '\0', 1024);
		while (fgets(line, sizeof(line), file)) {

			if ((strstr(line, "EXT-X-MEDIA:TYPE=AUDIO") != NULL)) {
				char *ptr = strstr(line, "URI=");
				ptr = ptr + 5;
				strcat(audio_playlist_path, ptr);

				int len = strlen(audio_playlist_path);
				audio_playlist_path[len - 2] = '\0';
				audio_playlist_found = 1;
				break;
			}

		}

		fclose(file);
	}

	if (audio_playlist_found) {
		return audio_playlist_path;
	}

	return NULL;

}

char *get_video_playlist(ngx_http_request_t *r, char *master_playlist_path) {

	int video_playlist_found = 0;
	char *video_playlist_path = (char*) malloc(PATH_MAX);
	memset(video_playlist_path, '\0', PATH_MAX);

	char *directory = get_dir(r, master_playlist_path);
	strcpy(video_playlist_path, directory);

	FILE* file = fopen(master_playlist_path, "r");

	if (file != NULL) {
		char line[1024];
		memset(line, '\0', 1024);

		while (fgets(line, sizeof(line), file)) {

			if ((strstr(line, "#EXT-X-STREAM-INF") != NULL)) {
				char *ptr = fgets(line, sizeof(line), file);
				strcat(video_playlist_path, ptr);

				int len = strlen(video_playlist_path);
				video_playlist_path[len - 1] = '\0';

				video_playlist_found = 1;
				break;
			}

		}

		fclose(file);
	}

	if (video_playlist_found) {
		return video_playlist_path;
	}

	return NULL;

}

char *get_subtitle_playlist(ngx_http_request_t *r, char *master_playlist_path) {
	int subtitle_playlist_found = 0;

	char *subtitle_playlist_path = (char*) malloc(PATH_MAX);
	memset(subtitle_playlist_path, '\0', PATH_MAX);

	char *directory = get_dir(r,master_playlist_path);
	strcpy(subtitle_playlist_path, directory);

	FILE* file = fopen(master_playlist_path, "r");

	if (file != NULL) {
		char line[1024];
		memset(line, '\0', 1024);

		while (fgets(line, sizeof(line), file)) {

			if ((strstr(line, "#EXT-X-MEDIA:TYPE=SUBTITLES") != NULL)) {
				char *ptr = strstr(line, "URI=");
				ptr = ptr + 5;
				strcat(subtitle_playlist_path, ptr);

				int len = strlen(subtitle_playlist_path);
				subtitle_playlist_path[len - 2] = '\0';

				subtitle_playlist_found = 1;
				break;
			}

		}

		fclose(file);
	}

	if (subtitle_playlist_found) {
		return subtitle_playlist_path;

	}
	return NULL;

}

void update_files(ngx_http_request_t *r, char *hls_media_playlist, List *files) {

	FILE* file = fopen(hls_media_playlist, "r");

	char *directory = get_dir(r, hls_media_playlist);
	if (file != NULL) {

		char line[1024];
		memset(line, '\0', 1024);

		while (fgets(line, sizeof(line), file)) {

			if (line[0] != '#') {

				char *file_path = (char *) malloc(PATH_MAX);
				memset(file_path, '\0', PATH_MAX);
				strcpy(file_path, directory);
				strcat(file_path, line);
				int len = strlen(file_path);
				file_path[len - 1] = '\0';

				add_to_list(files, (void*) file_path);
			}

		}

		fclose(file);
	}

}

void update_audio_files(ngx_http_request_t *r, char * master_playlist_path,
		List *audio_files) {

	char *audio_playlist_path = get_audio_playlist(r, master_playlist_path);

	if (audio_playlist_path != NULL) {
		update_files(r,audio_playlist_path, audio_files);

	}
}

void update_video_files(ngx_http_request_t *r, char * master_playlist_path,
		List *video_files) {

	char *video_playlist_path = get_video_playlist(r, master_playlist_path);

	if (video_playlist_path != NULL) {
		update_files(r,video_playlist_path, video_files);
	}
}

void update_subtitle_files(ngx_http_request_t *r, char * master_playlist_path,
		List *subtitle_files) {
	char *subtitle_playlist_path = get_subtitle_playlist(r,
			master_playlist_path);

	if (subtitle_playlist_path != NULL) {
		update_files(r,subtitle_playlist_path, subtitle_files);
	}
}

int check_list(char *input_file, List *files) {
	Node *ptr = files->head;

	while (ptr != NULL) {
		char *file = (char *) ptr->data;
		if (strcmp(input_file, file) == 0) {
			return 1;
		}
		ptr = ptr->next;

	}
	return 0;
}

void print_list(List *files) {

	Node *ptr = files->head;

	while (ptr != NULL) {
		char *file = (char *) ptr->data;
		printf("%s\n", file);
		ptr = ptr->next;
	}


}

int is_hls_playlist_found(ngx_http_request_t *r, char *path) {

	char *master_playlist_path = get_hls_master_playlist(r,path);

	if (master_playlist_path != NULL)
		return 1;
	return 0;

}

static enum stream_file_type sft=UNDEFINED;
enum stream_file_type *hls_get_file_type(ngx_http_request_t *r, char* path) {

	char *master_playlist_path = get_hls_master_playlist(r,path);

	List *audio_files = create_list();
	List *video_files = create_list();
	List *subtitle_files = create_list();

	update_audio_files(r, master_playlist_path, audio_files);
	update_video_files(r, master_playlist_path, video_files);
	update_subtitle_files(r, master_playlist_path, subtitle_files);

	if (check_list(path, audio_files)) {
		sft = AUDIO;

	} else if (check_list(path, video_files)) {
		sft = VIDEO;

	} else if (check_list(path, subtitle_files)) {
		sft = SUBTITLE;

	} else {
		sft = UNDEFINED;

	}
	return (enum stream_file_type*) &sft;

}

