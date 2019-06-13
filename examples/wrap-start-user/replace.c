struct link_map;
void __new__dl_init (struct link_map *main_map, int argc, char **argv, char **env);
void _dl_init (struct link_map *main_map, int argc, char **argv, char **env)
{
	__new__dl_init(main_map, argc, argv, env);
}
//asm ("__real__dl_init = .;");
