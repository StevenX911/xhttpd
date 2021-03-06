int get_line(int cfd, char *buf, int size);
char *get_file_type(const char *file);
void encode_str(char *to, int tosize, const char* from);
void decode_str(char *to, char *from);
int init_listen_fd(int port, int epfd);
void do_accept(int lfd, int epfd);
void disconnect(int cfd, int epfd);
void send_error(int cfd, int status, char *title, char *text);
void send_head(int cfd, int no, char *desc, char *ctype, int len);
void send_body(int cfd, const char *file);
void send_dir(int cfd, const char *dirname);
void http_request(int cfd, char *path);
void do_read(int lfd, int epfd);
void epoll_run(int port);