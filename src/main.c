// cat follow mouse real !!1!1!111!11!

#define _POSIX_C_SOURCE 200809L

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    const Point *frames;
    int len;
} SpriteSet;

typedef struct {
    GtkWidget *window;
    GdkPixbuf *sheet;
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkRectangle monitor_rect;
    GdkRectangle desktop_rect;
    char socket_path[108];
    double neko_x;
    double neko_y;
    double mouse_x;
    double mouse_y;
    bool have_mouse;
    bool mouse_moved;
    int frame_count;
    int idle_time;
    int idle_frame;
    int idle_animation;
    int sprite_x;
    int sprite_y;
} Neko;

enum {
    SPRITE_IDLE,
    SPRITE_ALERT,
    SPRITE_SCRATCH_SELF,
    SPRITE_SCRATCH_WALL_N,
    SPRITE_SCRATCH_WALL_S,
    SPRITE_SCRATCH_WALL_E,
    SPRITE_SCRATCH_WALL_W,
    SPRITE_TIRED,
    SPRITE_SLEEPING,
    SPRITE_N,
    SPRITE_NE,
    SPRITE_E,
    SPRITE_SE,
    SPRITE_S,
    SPRITE_SW,
    SPRITE_W,
    SPRITE_NW,
    SPRITE_COUNT
};

static const Point idle_frames[] = {{-3, -3}};
static const Point alert_frames[] = {{-7, -3}};
static const Point scratch_self_frames[] = {{-5, 0}, {-6, 0}, {-7, 0}};
static const Point scratch_wall_n_frames[] = {{0, 0}, {0, -1}};
static const Point scratch_wall_s_frames[] = {{-7, -1}, {-6, -2}};
static const Point scratch_wall_e_frames[] = {{-2, -2}, {-2, -3}};
static const Point scratch_wall_w_frames[] = {{-4, 0}, {-4, -1}};
static const Point tired_frames[] = {{-3, -2}};
static const Point sleeping_frames[] = {{-2, 0}, {-2, -1}};
static const Point n_frames[] = {{-1, -2}, {-1, -3}};
static const Point ne_frames[] = {{0, -2}, {0, -3}};
static const Point e_frames[] = {{-3, 0}, {-3, -1}};
static const Point se_frames[] = {{-5, -1}, {-5, -2}};
static const Point s_frames[] = {{-6, -3}, {-7, -2}};
static const Point sw_frames[] = {{-5, -3}, {-6, -1}};
static const Point w_frames[] = {{-4, -2}, {-4, -3}};
static const Point nw_frames[] = {{-1, 0}, {-1, -1}};

static const SpriteSet sprites[SPRITE_COUNT] = {
    [SPRITE_IDLE] = {idle_frames, 1},
    [SPRITE_ALERT] = {alert_frames, 1},
    [SPRITE_SCRATCH_SELF] = {scratch_self_frames, 3},
    [SPRITE_SCRATCH_WALL_N] = {scratch_wall_n_frames, 2},
    [SPRITE_SCRATCH_WALL_S] = {scratch_wall_s_frames, 2},
    [SPRITE_SCRATCH_WALL_E] = {scratch_wall_e_frames, 2},
    [SPRITE_SCRATCH_WALL_W] = {scratch_wall_w_frames, 2},
    [SPRITE_TIRED] = {tired_frames, 1},
    [SPRITE_SLEEPING] = {sleeping_frames, 2},
    [SPRITE_N] = {n_frames, 2},
    [SPRITE_NE] = {ne_frames, 2},
    [SPRITE_E] = {e_frames, 2},
    [SPRITE_SE] = {se_frames, 2},
    [SPRITE_S] = {s_frames, 2},
    [SPRITE_SW] = {sw_frames, 2},
    [SPRITE_W] = {w_frames, 2},
    [SPRITE_NW] = {nw_frames, 2},
};

static void set_sprite(Neko *app, int name, int frame) {
    const SpriteSet *set = &sprites[name];
    Point sprite = set->frames[frame % set->len];
    app->sprite_x = sprite.x;
    app->sprite_y = sprite.y;
}

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    Neko *app = data;
    (void)widget;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    gdk_cairo_set_source_pixbuf(cr, app->sheet, app->sprite_x * 32, app->sprite_y * 32);
    cairo_rectangle(cr, 0, 0, 32, 32);
    cairo_fill(cr);
    return FALSE;
}

static void shape_input(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *window = gtk_widget_get_window(widget);
    cairo_region_t *region = cairo_region_create();
    gdk_window_input_shape_combine_region(window, region, 0, 0);
    gdk_window_set_pass_through(window, TRUE);
    cairo_region_destroy(region);
}

static bool set_socket_path(Neko *app) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!runtime || !sig) return false;
    int n = snprintf(app->socket_path, sizeof(app->socket_path), "%s/hypr/%s/.socket.sock", runtime, sig);
    return n > 0 && (size_t)n < sizeof(app->socket_path);
}

static bool read_cursor(Neko *app) {
    if (!app->socket_path[0]) return false;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t path_len = strlen(app->socket_path);
    if (path_len >= sizeof(addr.sun_path)) {
        close(fd);
        return false;
    }
    memcpy(addr.sun_path, app->socket_path, path_len + 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }
    const char command[] = "cursorpos";
    if (write(fd, command, sizeof(command) - 1) < 0) {
        close(fd);
        return false;
    }
    char buffer[128];
    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (len <= 0) return false;
    buffer[len] = 0;
    double x;
    double y;
    if (sscanf(buffer, "%lf, %lf", &x, &y) != 2) return false;
    app->mouse_moved = app->have_mouse && (fabs(x - app->mouse_x) > 0.5 || fabs(y - app->mouse_y) > 0.5);
    app->have_mouse = true;
    app->mouse_x = x;
    app->mouse_y = y;
    return true;
}

static void update_monitor(Neko *app) {
    int count = gdk_display_get_n_monitors(app->display);
    GdkMonitor *chosen = app->monitor;
    GdkRectangle rect = app->monitor_rect;
    bool have_desktop = false;
    GdkRectangle desktop = {0, 0, 0, 0};
    for (int i = 0; i < count; i++) {
        GdkMonitor *monitor = gdk_display_get_monitor(app->display, i);
        GdkRectangle r;
        gdk_monitor_get_geometry(monitor, &r);
        if (!have_desktop) {
            desktop = r;
            have_desktop = true;
        } else {
            int left = MIN(desktop.x, r.x);
            int top = MIN(desktop.y, r.y);
            int right = MAX(desktop.x + desktop.width, r.x + r.width);
            int bottom = MAX(desktop.y + desktop.height, r.y + r.height);
            desktop.x = left;
            desktop.y = top;
            desktop.width = right - left;
            desktop.height = bottom - top;
        }
        if (app->neko_x >= r.x && app->neko_x < r.x + r.width && app->neko_y >= r.y && app->neko_y < r.y + r.height) {
            chosen = monitor;
            rect = r;
        }
    }
    if (have_desktop) app->desktop_rect = desktop;
    if (chosen && chosen != app->monitor) {
        app->monitor = chosen;
        app->monitor_rect = rect;
        if (app->window) gtk_layer_set_monitor(GTK_WINDOW(app->window), app->monitor);
    } else if (chosen) {
        app->monitor_rect = rect;
    }
}

static void move_window(Neko *app) {
    update_monitor(app);
    int x = (int)round(app->neko_x - app->monitor_rect.x - 16);
    int y = (int)round(app->neko_y - app->monitor_rect.y - 16);
    gtk_layer_set_margin(GTK_WINDOW(app->window), GTK_LAYER_SHELL_EDGE_LEFT, x);
    gtk_layer_set_margin(GTK_WINDOW(app->window), GTK_LAYER_SHELL_EDGE_TOP, y);
    gtk_widget_queue_draw(app->window);
}

static void reset_idle(Neko *app) {
    app->idle_animation = -1;
    app->idle_frame = 0;
}

static void idle(Neko *app) {
    bool slow_tick = app->frame_count % 2 == 0;
    if (slow_tick) app->idle_time++;
    if (slow_tick && app->idle_time > 10 && rand() % 200 == 0 && app->idle_animation < 0) {
        int choices[6] = {SPRITE_SLEEPING, SPRITE_SCRATCH_SELF};
        int count = 2;
        if (app->neko_x < app->desktop_rect.x + 32) choices[count++] = SPRITE_SCRATCH_WALL_W;
        if (app->neko_y < app->desktop_rect.y + 32) choices[count++] = SPRITE_SCRATCH_WALL_N;
        if (app->neko_x > app->desktop_rect.x + app->desktop_rect.width - 32) choices[count++] = SPRITE_SCRATCH_WALL_E;
        if (app->neko_y > app->desktop_rect.y + app->desktop_rect.height - 32) choices[count++] = SPRITE_SCRATCH_WALL_S;
        app->idle_animation = choices[rand() % count];
    }
    switch (app->idle_animation) {
        case SPRITE_SLEEPING:
            if (app->idle_frame < 8) {
                set_sprite(app, SPRITE_TIRED, 0);
            } else {
                set_sprite(app, SPRITE_SLEEPING, app->idle_frame / 4);
            }
            if (app->idle_frame > 192) reset_idle(app);
            break;
        case SPRITE_SCRATCH_WALL_N:
        case SPRITE_SCRATCH_WALL_S:
        case SPRITE_SCRATCH_WALL_E:
        case SPRITE_SCRATCH_WALL_W:
        case SPRITE_SCRATCH_SELF:
            set_sprite(app, app->idle_animation, app->idle_frame);
            if (app->idle_frame > 9) reset_idle(app);
            break;
        default:
            set_sprite(app, SPRITE_IDLE, 0);
            return;
    }
    if (slow_tick) app->idle_frame++;
}

static void frame(Neko *app) {
    const double speed = 5.0;
    app->frame_count++;
    double diff_x = app->neko_x - app->mouse_x;
    double diff_y = app->neko_y - app->mouse_y;
    double distance = sqrt(diff_x * diff_x + diff_y * diff_y);
    if (distance < speed || distance < 48.0) {
        idle(app);
        return;
    }
    reset_idle(app);
    if (app->mouse_moved) app->idle_time = 0;
    if (app->idle_time > 1) {
        set_sprite(app, SPRITE_ALERT, 0);
        if (app->idle_time > 7) app->idle_time = 7;
        app->idle_time--;
        return;
    }
    bool north = diff_y / distance > 0.5;
    bool south = diff_y / distance < -0.5;
    bool west = diff_x / distance > 0.5;
    bool east = diff_x / distance < -0.5;
    int direction = SPRITE_IDLE;
    if (north && east) direction = SPRITE_NE;
    else if (south && east) direction = SPRITE_SE;
    else if (south && west) direction = SPRITE_SW;
    else if (north && west) direction = SPRITE_NW;
    else if (north) direction = SPRITE_N;
    else if (east) direction = SPRITE_E;
    else if (south) direction = SPRITE_S;
    else if (west) direction = SPRITE_W;
    set_sprite(app, direction, app->frame_count / 2);
    app->neko_x -= (diff_x / distance) * speed;
    app->neko_y -= (diff_y / distance) * speed;
    double min_x = app->desktop_rect.x + 16;
    double min_y = app->desktop_rect.y + 16;
    double max_x = app->desktop_rect.x + app->desktop_rect.width - 16;
    double max_y = app->desktop_rect.y + app->desktop_rect.height - 16;
    if (app->neko_x < min_x) app->neko_x = min_x;
    if (app->neko_y < min_y) app->neko_y = min_y;
    if (app->neko_x > max_x) app->neko_x = max_x;
    if (app->neko_y > max_y) app->neko_y = max_y;
}

static gboolean tick(gpointer data) {
    Neko *app = data;
    app->mouse_moved = false;
    read_cursor(app);
    frame(app);
    move_window(app);
    return G_SOURCE_CONTINUE;
}

static char *asset_path(const char *argv0) {
    if (g_file_test("assets/oneko.gif", G_FILE_TEST_EXISTS)) return g_strdup("assets/oneko.gif");
    if (g_file_test("oneko.gif", G_FILE_TEST_EXISTS)) return g_strdup("oneko.gif");
    char *dir = g_path_get_dirname(argv0);
    char *nearby = g_build_filename(dir, "assets", "oneko.gif", NULL);
    g_free(dir);
    if (g_file_test(nearby, G_FILE_TEST_EXISTS)) return nearby;
    g_free(nearby);
    return g_strdup("/usr/local/share/oneko-hypr/oneko.gif");
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    Neko app;
    memset(&app, 0, sizeof(app));
    app.idle_animation = -1;
    app.neko_x = 32;
    app.neko_y = 32;
    app.mouse_x = 0;
    app.mouse_y = 0;
    srand((unsigned int)time(NULL));
    char *path = argc > 1 ? g_strdup(argv[1]) : asset_path(argv[0]);
    GError *error = NULL;
    app.sheet = gdk_pixbuf_new_from_file(path, &error);
    if (!app.sheet) {
        g_printerr("failed to load %s: %s\n", path, error ? error->message : "unknown error");
        g_clear_error(&error);
        g_free(path);
        return 1;
    }
    g_free(path);
    if (!set_socket_path(&app)) {
        g_printerr("HYPRLAND_INSTANCE_SIGNATURE is not set\n");
        g_object_unref(app.sheet);
        return 1;
    }
    app.display = gdk_display_get_default();
    app.monitor = gdk_display_get_primary_monitor(app.display);
    if (!app.monitor) app.monitor = gdk_display_get_monitor(app.display, 0);
    if (!app.monitor) {
        g_printerr("no monitor found\n");
        g_object_unref(app.sheet);
        return 1;
    }
    gdk_monitor_get_geometry(app.monitor, &app.monitor_rect);
    app.desktop_rect = app.monitor_rect;
    read_cursor(&app);
    app.neko_x = app.mouse_x + 32;
    app.neko_y = app.mouse_y + 32;
    update_monitor(&app);
    set_sprite(&app, SPRITE_IDLE, 0);
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(app.window), 32, 32);
    gtk_widget_set_size_request(app.window, 32, 32);
    gtk_widget_set_app_paintable(app.window, TRUE);
    gtk_layer_init_for_window(GTK_WINDOW(app.window));
    gtk_layer_set_namespace(GTK_WINDOW(app.window), "oneko-hypr");
    gtk_layer_set_layer(GTK_WINDOW(app.window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(app.window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(app.window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_monitor(GTK_WINDOW(app.window), app.monitor);
    g_signal_connect(app.window, "draw", G_CALLBACK(draw), &app);
    g_signal_connect(app.window, "realize", G_CALLBACK(shape_input), NULL);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(app.window);
    move_window(&app);
    g_timeout_add(50, tick, &app);
    gtk_main();
    g_object_unref(app.sheet);
    return 0;
}
