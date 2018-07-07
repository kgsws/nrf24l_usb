#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>
#include <gtk/gtk.h>
#include "protocol.h"

//#define USE_SCAN

#ifdef USE_SCAN
// image
#include "graph.h"
#endif

#define TITLE	"kgsws' nRF24L01 control"
#define NUM_TX	5

#define SAVE_MAGIC1	0x0046526e
#define SAVE_MAGIC2	0x02676663

typedef struct
{
	uint8_t addr[5];
	uint8_t len;
} __attribute__((packed)) saveaddr_t;

typedef struct
{
	uint32_t magic1;
	uint32_t magic2;
} __attribute__((packed)) save_t;

char hextab[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

// main window
GtkWidget *win_main;
GdkPixbuf *app_icon;
GtkWidget *nb_main;
PangoFontDescription *entry_font;

// pages
char *page_name[] =
{
	"Device",
	"Transmit",
	"RxPipe0",
	"RxPipe1",
	"RxPipe2",
	"RxPipe3",
	"RxPipe4",
	"RxPipe5",
#ifdef USE_SCAN
	"Scan",
#endif
};
GtkWidget *page_box[sizeof(page_name)/sizeof(char*)];
// pipes with new data
char *pipe_name[] =
{
	"<u>RxPipe0</u>",
	"<u>RxPipe1</u>",
	"<u>RxPipe2</u>",
	"<u>RxPipe3</u>",
	"<u>RxPipe4</u>",
	"<u>RxPipe5</u>",
};

// pipe list
GtkWidget *pipe_label[6];
int cur_page = -1;
// device
struct usb_device *dev_selected;
GtkWidget *dev_entry;
GtkWidget *dev_scan;
GtkListStore *dev_store;
GtkWidget *entry_rxaddr[6];
GtkWidget *spin_rxaddr[6];
GtkWidget *check_rxaddr[6];
GtkWidget *crc_spin;
GtkWidget *channel_spin;
GtkWidget *adw_spin;
GtkWidget *rate_spin;
GtkWidget *dev_btn[5];
GtkWidget *con_text;
GtkWidget *rx_switch;
char base_addr[5];
// TX
GtkWidget *entry_txaddr[NUM_TX];
GtkWidget *entry_txdata[NUM_TX];
GtkWidget *tx_btn[NUM_TX];
// RX
GtkListStore *rx_list[6];
int rxmode = 0;
// current settings
setconf_t curconf;
setpipe_t curpipes;
int postinit = 0;
// scan
#ifdef USE_SCAN
GdkPixbuf *scan_pixbuf;
GtkWidget *scan_btn;
GtkWidget *scan_widget;
#endif

void run_dialog(GtkWidget *dialog)
{
	gtk_window_set_title(GTK_WINDOW(dialog), TITLE);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void enable_port()
{
	// scan button
	gtk_widget_set_sensitive(dev_scan, FALSE);
	// entry
	gtk_widget_set_sensitive(dev_entry, FALSE);
	// label
	gtk_label_set_text(GTK_LABEL(con_text), "Disconnect");
	// no Rx mode
	rxmode = 0;
	gtk_switch_set_active(GTK_SWITCH(rx_switch), FALSE);
}

void disable_port()
{
	int i;
	// label
	gtk_label_set_text(GTK_LABEL(con_text), "Connect");
	// entry
	gtk_widget_set_sensitive(dev_entry, TRUE);
	// buttons
	for(i = 2; i < 5; i++)
		gtk_widget_set_sensitive(dev_btn[i], FALSE);
	// Tx buttons
	for(i = 0; i < NUM_TX; i++)
		gtk_widget_set_sensitive(tx_btn[i], FALSE);
	// Rx switch
	gtk_widget_set_sensitive(rx_switch, FALSE);
	// device scan
	gtk_widget_set_sensitive(dev_scan, TRUE);
	// scan button
#ifdef USE_SCAN
	gtk_widget_set_sensitive(scan_btn, FALSE);
#endif
}

static void read_cb(GtkWidget *widget, void *unused)
{
	int i;
	// disable buttons
	for(i = 1; i < 5; i++)
		gtk_widget_set_sensitive(dev_btn[i], FALSE);
	// Rx switch
	gtk_widget_set_sensitive(rx_switch, FALSE);
	// scan
#ifdef USE_SCAN
	gtk_widget_set_sensitive(scan_btn, FALSE);
#endif

	postinit = 0;
	// read registers
	P_GetSettings();
}

char *getAddrString(uint8_t *data)
{
	int i;
	static char ret[12] = "0011223344";
	char *dst = ret;

	data += 4;
	for(i = 0; i < 5; i++)
	{
		*dst = hextab[*data >> 4];
		dst++;
		*dst = hextab[*data & 15];
		dst++;
		data--;
	}

	return ret;
}

uint8_t *getStringAddr(const char *text)
{
	static uint8_t ret[sizeof(uint64_t)];

	memset(ret, 0, sizeof(ret));
	sscanf(text, "%lX", ret);

	return ret;
}

static void write_cb(GtkWidget *widget, void *unused)
{
	int i;
	const char *text;
	uint8_t *addr;
	int channel = gtk_spin_button_get_value_as_int((GtkSpinButton*)channel_spin);
	int rate = gtk_spin_button_get_value_as_int((GtkSpinButton*)rate_spin);
	int crc = gtk_spin_button_get_value_as_int((GtkSpinButton*)crc_spin);
	int adw = gtk_spin_button_get_value_as_int((GtkSpinButton*)adw_spin);

	// disable WRITE
	gtk_widget_set_sensitive(dev_btn[4], FALSE);

	curconf.channel = channel;
	curconf.rate = rate;
	curconf.crc = crc;
	curconf.adw = adw;
	P_SetSettings(channel, rate, crc, adw);

	for(i = 0; i < 6; i++)
	{
		text = gtk_entry_get_text(GTK_ENTRY(entry_rxaddr[i]));
		addr = getStringAddr(text);
		if(i < 2)
			memcpy(base_addr, addr, 5);
		else
			base_addr[0] = addr[0];
		gtk_entry_set_text(GTK_ENTRY(entry_rxaddr[i]), getAddrString(base_addr));
		memcpy(curpipes.pipe[i].addr, addr, 5);
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rxaddr[i])))
			curpipes.pipe[i].size = 0xFF;
		else
			curpipes.pipe[i].size = gtk_spin_button_get_value_as_int((GtkSpinButton*)spin_rxaddr[i]);
	}

	P_SetPipes(&curpipes);
}

gboolean StatusGtk(void *got)
{
	int status = (int)got;
	int i;
	GtkWidget *dialog = NULL;

	if(status)
	{
		// close port
		disable_port();
		// disconnect
	}

	switch(status)
	{
		case 0:
			dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed to open device.");
		break;
		case 1:
			dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed to claim interface.");
		break;
	}

	if(dialog)
		run_dialog(dialog);

	return FALSE;
}

gboolean ConfigGtk(setconf_t *got)
{
	gtk_spin_button_set_value((GtkSpinButton*)channel_spin, got->channel);
	gtk_spin_button_set_value((GtkSpinButton*)rate_spin, got->rate);
	gtk_spin_button_set_value((GtkSpinButton*)crc_spin, got->crc);
	gtk_spin_button_set_value((GtkSpinButton*)adw_spin, got->adw);
	memcpy(&curconf, got, sizeof(setconf_t));
	free(got);

	P_GetPipes();

	return FALSE;
}

gboolean PipesGtk(setpipe_t *got)
{
	int i;

	for(i = 0; i < 6; i++)
	{
		if(got->pipe[i].size > 32)
		{
			gtk_widget_set_sensitive(spin_rxaddr[i], FALSE);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rxaddr[i]), TRUE);
		} else {
			gtk_spin_button_set_value((GtkSpinButton*)spin_rxaddr[i], got->pipe[i].size);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rxaddr[i]), FALSE);
		}
		gtk_entry_set_text(GTK_ENTRY(entry_rxaddr[i]), getAddrString(got->pipe[i].addr));
	}

	memcpy(&curpipes, got, sizeof(setpipe_t));

	free(got);

	// enable buttons
	for(i = 1; i < 5; i++)
		if(i != 4) // no WRITE
			gtk_widget_set_sensitive(dev_btn[i], TRUE);
	// Tx buttons
	for(i = 0; i < NUM_TX; i++)
		gtk_widget_set_sensitive(tx_btn[i], TRUE);
	// Rx switch
	gtk_widget_set_sensitive(rx_switch, TRUE);
	// scan button
#ifdef USE_SCAN
	gtk_widget_set_sensitive(scan_btn, TRUE);
#endif

	postinit = 1;

	return FALSE;
}

static void usbscan_cb(GtkWidget *widget, GtkWidget *entry)
{
	gtk_widget_set_sensitive(dev_btn[0], FALSE);
	gtk_list_store_clear(dev_store);
	P_Scan();
}

static void connect_cb(GtkWidget *widget, GtkWidget *entry)
{
	if(handle)
	{
		postinit = 0;
		rxmode = 0;
		disable_port();
		P_Deinit();
	} else
	if(dev_selected)
	{
		// connect
		if(P_Init(dev_selected))
			disable_port();
		else
			enable_port();
	}
}

int getHex(char in)
{
	if(in >= '0' && in <= '9')
		return in - '0';
	if(in >= 'A' && in <= 'F')
		return (in - 'A') + 10;
	if(in >= 'a' && in <= 'f')
		return (in - 'a') + 10;

	return -1;
}

static void transmit_cb(GtkWidget *widget, void *got)
{
	int num = (int)got;
	int i, count;
	const char *addr;
	const char *data;
	uint8_t data_out[32];
	char text_out[32*3];
	uint8_t *dst = data_out;
	uint8_t *dstend = data_out + sizeof(data_out);
	int state = 0;

	// load
	addr = gtk_entry_get_text(GTK_ENTRY(entry_txaddr[num]));
	data = gtk_entry_get_text(GTK_ENTRY(entry_txdata[num]));
	// process
	while(*data)
	{
		if(state)
		{
			i = getHex(*data);
			if(i >= 0)
			{
				*dst <<= 4;
				*dst |= i;
			}
			dst++;
			if(dst == dstend)
				break;
			state = 0;
		} else {
			// find first valid character
			i = getHex(*data);
			if(i >= 0)
			{
				state = 1;
				*dst = i;
			}
		}
		data++;
	}
	if(state)
		dst++;
	count = dst - data_out;
	if(count)
	{
		// convert
		dst = text_out;
		for(i = 0; i < count; i++)
		{
			if(i)
			{
				*dst = ' ';
				dst++;
			}
			*dst = hextab[data_out[i] >> 4];
			dst++;
			*dst = hextab[data_out[i] & 15];
			dst++;
		}
		*dst = 0;
		gtk_entry_set_text(GTK_ENTRY(entry_txdata[num]), text_out);
		// fix address
		dst = getStringAddr(addr);
		gtk_entry_set_text(GTK_ENTRY(entry_txaddr[num]), getAddrString(dst));
		// DO IT
		P_TxAddress(dst);
		P_TxData(data_out, count);
	}
}

static void rx_sw_cb(GtkSwitch *sw, GParamSpec *pspec, void *unused)
{
	uint8_t reg;
	int state = gtk_switch_get_active(sw);
	if(rxmode != state)
	{
		rxmode = state;
		P_RxMode(rxmode);
	}
}

gboolean AddPacketGtk(packet_t *got)
{
	int i = got->len;
	uint8_t *src = got->data;
	GtkTreeIter iter;
	const char *addr;
	char text[32*3];
	char *dst = text;
	char timetext[32];
	time_t ts;
	struct tm *tm;

	// data
	while(i--)
	{
		*dst = hextab[*src >> 4];
		dst++;
		*dst = hextab[*src & 15];
		dst++;
		if(i)
		{
			*dst = ' ';
			dst++;
		}
		src++;
	}
	*dst = 0;
	// address
	addr = gtk_entry_get_text(GTK_ENTRY(entry_rxaddr[got->pipe]));
//	addr += (5 - addrlen) * 2;
	// time
	time(&ts);
	tm = localtime(&ts);
	sprintf(timetext, "%02i:%02i:%02i", tm->tm_hour, tm->tm_min, tm->tm_sec);
	// fill
	gtk_list_store_prepend(rx_list[got->pipe], &iter);
	gtk_list_store_set(rx_list[got->pipe], &iter, 0, timetext, 1, addr, 2, text, -1);
	// notify pipe
	if(cur_page != got->pipe + 2)
		gtk_label_set_markup(GTK_LABEL(pipe_label[got->pipe]), pipe_name[got->pipe]);
	// done
	free(got);
	return FALSE;
}

// Rx View
void init_list_rx(GtkWidget *widget, int num)
{
	GtkCellRenderer    *text;
	GtkTreeViewColumn  *column;

	// timestamp
	text = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Time", text, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
	// pipe address
	text = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Address", text, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
	// pipe data
	text = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Payload", text, "text", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
	// list itself
	rx_list[num] = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(rx_list[num]));
	gtk_widget_override_font(widget, entry_font);
	g_object_unref(rx_list[num]);
}

void device_cb(GtkComboBox *widget, gpointer user_data)
{
	int i;

	i = gtk_combo_box_get_active(widget);
	if(i < 0 || i >= devices_count)
	{
		gtk_widget_set_sensitive(dev_btn[0], FALSE);
		return;
	}
	dev_selected = device_list[i];
	gtk_widget_set_sensitive(dev_btn[0], !!device_list[i]);
}

void init_list_dev()
{
	GtkCellRenderer *column;

	dev_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
//	gtk_list_store_insert_with_values(dev_store, NULL, -1, 0, "gray", 1, "select device", -1);
	dev_entry = gtk_combo_box_new_with_model(GTK_TREE_MODEL(dev_store));
	g_signal_connect(dev_entry, "changed", G_CALLBACK(device_cb), NULL);
	g_object_unref(dev_store);
	column = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dev_entry), column, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dev_entry), column, "cell-background", 0, "text", 1, NULL);
}

// notebook page
void page_cb(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data)
{
	cur_page = page_num;
	if(page_num < 2 || page_num > 7)
		return;
	// modify label
	gtk_label_set_text(GTK_LABEL(pipe_label[page_num - 2]), page_name[page_num]);
}

char *FileSelection(char *text, char *save)
{
	GtkWidget *dialog;
	char *filename;

	if(save)
	{
		dialog = gtk_file_chooser_dialog_new(text, GTK_WINDOW(win_main), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), save);
	} else {
		dialog = gtk_file_chooser_dialog_new(text, GTK_WINDOW(win_main), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), ".");
	}
	if(gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_widget_destroy(dialog);
		return filename;
	}
	gtk_widget_destroy(dialog);
	return NULL;
}

void file_cb(GtkWidget *widget, void *got)
{
	int i;
	save_t save;
	FILE *f;
	char *file;
	uint8_t *addr;
	const char *text;
	uint8_t out;

	setconf_t tempconf;
	setpipe_t tempipes;

	if(got)
	{
		file = FileSelection("Save nRF24L01 config.", "config.bin");
		if(!file)
			return;
		// save
		f = fopen(file, "wb");
		g_free(file);
		if(!f)
			return;

		// save MAGIC
		save.magic1 = SAVE_MAGIC1;
		save.magic2 = SAVE_MAGIC2;
		fwrite(&save, 1, sizeof(save_t), f);
		// save config
		fwrite(&curconf, 1, sizeof(setconf_t), f);
		// save pipes
		fwrite(&curpipes, 1, sizeof(setpipe_t), f);
		// save Tx
		//// TODO
		// close
		fclose(f);
	} else {
		file = FileSelection("Load nRF24L01 config.", NULL);
		if(!file)
			return;
		// load
		f = fopen(file, "rb");
		g_free(file);
		if(!f)
			return;
		// read magic
		if(fread(&save, 1, sizeof(save_t), f) != sizeof(save))
		{
			fclose(f);
			return;
		}
		// check magic
		if(save.magic1 != SAVE_MAGIC1 || save.magic2 != SAVE_MAGIC2)
		{
			fclose(f);
			return;
		}
		// disable Rx mode
		if(rxmode)
		{
			P_RxMode(FALSE);
			gtk_switch_set_active(GTK_SWITCH(rx_switch), FALSE);
			rxmode = 0;
		}
		// read config
		if(fread(&tempconf, 1, sizeof(setconf_t), f) != sizeof(setconf_t))
		{
			fclose(f);
			return;
		}
		// read pipes
		if(fread(&tempipes, 1, sizeof(setpipe_t), f) != sizeof(setpipe_t))
		{
			fclose(f);
			return;
		}
		// close
		fclose(f);
		// copy
		memcpy(&curconf, &tempconf, sizeof(setconf_t));
		memcpy(&curpipes, &tempipes, sizeof(setpipe_t));
		// apply
		P_SetSettings(curconf.channel, curconf.rate, curconf.crc, curconf.adw);
		P_SetPipes(&curpipes);
		// wait
		usleep(50*1000);
		// do init again to read values
		postinit = 0;
		P_GetSettings();
	}
}
#ifdef USE_SCAN
gboolean UpdateScanGtk(uint8_t *data)
{
	uint32_t *pix;
	int stride = gdk_pixbuf_get_rowstride(scan_pixbuf) / 4;

	uint32_t line[128 * 3];
	uint32_t *dst = line;
	uint8_t col;

	int i;
	uint8_t *src = data;

	// process data
	for(i = 0; i < 128; i++)
	{
		if(i & 1)
		{
			col = *src >> 4;
			if(col > 3)
				col = 3;
			col = 255 - (col * 85);
//			col = 255 - ((*src >> 4) * 17);
			*dst = col | (col << 8) | (col << 16) | 0xFF000000;
			dst[1] = *dst;
			dst[2] = *dst;
			dst += 3;
			src++;
		} else {
			col = *src & 15;
			if(col > 3)
				col = 3;
			col = 255 - (col * 85);
//			col = 255 - ((*src & 15) * 17);
			*dst = col | (col << 8) | (col << 16) | 0xFF000000;
			dst[1] = *dst;
			dst[2] = *dst;
			dst += 3;
		}
	}

	// advance graph time
	pix = (uint32_t*)gdk_pixbuf_get_pixels(scan_pixbuf);
	pix += (512 - 63 - 384) * stride + 64;
	for(i = 0; i < 384; i++)
	{
		memcpy(pix, pix + (6*stride), sizeof(line));
		pix += stride;
	}

	// add new line
	pix = (uint32_t*)gdk_pixbuf_get_pixels(scan_pixbuf);
	pix += (512 - 64) * stride + 64;
	for(i = 0; i < 6; i++)
	{
		memcpy(pix, line, sizeof(line));
		pix -= stride;
	}

	gtk_widget_hide(scan_widget);
	gtk_widget_show(scan_widget);

	free(data);

	// all buttons
	for(i = 1; i < 5; i++)
		gtk_widget_set_sensitive(dev_btn[i], TRUE);
	// Tx buttons
	for(i = 0; i < NUM_TX; i++)
		gtk_widget_set_sensitive(tx_btn[i], TRUE);
	// Rx switch
	gtk_widget_set_sensitive(rx_switch, TRUE);
	// scan button
	gtk_widget_set_sensitive(scan_btn, TRUE);

	return FALSE;
}

void scan_cb(GtkWidget *widget, void *unused)
{
	int i;
	// all buttons
	for(i = 1; i < 5; i++)
		gtk_widget_set_sensitive(dev_btn[i], FALSE);
	// Tx buttons
	for(i = 0; i < NUM_TX; i++)
		gtk_widget_set_sensitive(tx_btn[i], FALSE);
	// Rx switch
	gtk_widget_set_sensitive(rx_switch, FALSE);
	// scan button
	gtk_widget_set_sensitive(scan_btn, FALSE);
	// disable RX mode
	if(rxmode)
	{
		rxmode = 0;
		gtk_switch_set_active(GTK_SWITCH(rx_switch), FALSE);
	}
	// do it
	P_DoScan();
}
#endif
void toggle_cb(GtkWidget *widget, void *get)
{
	int num = (int)get;
	int state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	gtk_widget_set_sensitive(spin_rxaddr[num], !state);

	// enable WRITE
	gtk_widget_set_sensitive(dev_btn[4], TRUE);
}

void spin_cb(GtkSpinButton *spin_button, gpointer user_data)
{
	if(postinit)
	// enable WRITE
	gtk_widget_set_sensitive(dev_btn[4], TRUE);
}

void entry_cb(GtkEntry *entry, gchar *preedit, gpointer  user_data)
{
	if(postinit)
	// enable WRITE
	gtk_widget_set_sensitive(dev_btn[4], TRUE);
}

int main(int argc, char **argv)
{
	int i;
	GtkWidget *widget;//, *widget2;
	GtkWidget *box[4];
	GtkTreeSelection *selection;
	GList *list;

	// init libUSB
	usb_init();
	// init GTK
	gtk_init(&argc, &argv);

	// load icon
	app_icon = gdk_pixbuf_new_from_file("icon.png", NULL);

	// prepare font
	entry_font = pango_font_description_from_string("monospace");

	// main window
	win_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win_main), TITLE);
	g_signal_connect(G_OBJECT(win_main), "destroy", G_CALLBACK(gtk_main_quit), NULL);
//	gtk_window_set_resizable(GTK_WINDOW(win_main), FALSE);
	gtk_widget_set_size_request(win_main, 800, 600);
	gtk_window_set_position(GTK_WINDOW(win_main), GTK_WIN_POS_CENTER);
	// set icon
	if(app_icon)
		gtk_window_set_icon(GTK_WINDOW(win_main), app_icon);

	// main notebook
	nb_main = gtk_notebook_new();
	gtk_container_add(GTK_CONTAINER(win_main), nb_main);
	// fill pages with boxes
	for(i = 0; i < sizeof(page_name)/sizeof(char*); i++)
	{
		if(i < 2 || i > 7)
		{
			// create VBOX
			page_box[i] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
			// label
			widget = gtk_label_new(page_name[i]);
		} else {
			// create scrollable
			page_box[i] = gtk_scrolled_window_new(NULL, NULL);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page_box[i]), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
			// create Rx view
			widget = gtk_tree_view_new();
			init_list_rx(widget, i - 2);
			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(page_box[i]), widget);
			// label
			pipe_label[i - 2] = widget = gtk_label_new(page_name[i]);
		}
		// add page
		gtk_notebook_append_page(GTK_NOTEBOOK(nb_main), page_box[i], widget);
	}
	g_signal_connect(nb_main, "switch-page", G_CALLBACK(page_cb), NULL);

	//////////////////////////////////////
	//// device tab
	//////////////////////////////////////
	// hbox
	box[0] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(page_box[0]), box[0], TRUE, FALSE, 0);
	// vbox
	box[3] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 32);
	gtk_box_pack_start(GTK_BOX(box[0]), box[3], TRUE, FALSE, 0);
	// device file HBOX
	box[0] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box[3]), box[0], FALSE, FALSE, 0);
	// device scan button
	dev_scan = gtk_button_new_with_label("Scan");
	g_signal_connect(dev_scan, "clicked", G_CALLBACK(usbscan_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[0]), dev_scan, TRUE, FALSE, 0);
	// device selection
	init_list_dev();
	gtk_box_pack_start(GTK_BOX(box[0]), dev_entry, TRUE, FALSE, 0);
	// connect / disconnect button
	dev_btn[0] = gtk_button_new_with_label("Connect");
	gtk_widget_set_sensitive(dev_btn[0], FALSE);
	list = gtk_container_get_children(GTK_CONTAINER(dev_btn[0]));
	con_text = list->data;
	g_list_free(list);
	g_signal_connect(dev_btn[0], "clicked", G_CALLBACK(connect_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[0]), dev_btn[0], TRUE, TRUE, 0);
	// PIPES / SETTINGS
	box[2] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
	gtk_box_pack_start(GTK_BOX(box[3]), box[2], FALSE, FALSE, 0);
	// 6x pipe address + packet length
	box[0] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_box_pack_start(GTK_BOX(box[2]), box[0], FALSE, FALSE, 0);
	// settings label
	widget = gtk_label_new("Rx Pipe settings");
	gtk_box_pack_start(GTK_BOX(box[0]), widget, FALSE, FALSE, 0);
	// HBOX - info text
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box[0]), box[1], TRUE, FALSE, 0);
	// infotext itself
	widget = gtk_label_new("Rx Address");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	widget = gtk_label_new("Rx Length");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	widget = gtk_label_new("Dyn");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	// all fields
	for(i = 0; i < 6; i++)
	{
		// HBOX
		box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(box[0]), box[1], FALSE, FALSE, 0);
		// entry
		entry_rxaddr[i] = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry_rxaddr[i]), 10);
		gtk_widget_override_font(entry_rxaddr[i], entry_font);
		g_signal_connect(entry_rxaddr[i], "changed", G_CALLBACK(entry_cb), NULL);
		gtk_box_pack_start(GTK_BOX(box[1]), entry_rxaddr[i], TRUE, FALSE, 0);
		// spin entry
		spin_rxaddr[i] = gtk_spin_button_new_with_range(0, 32, 1);
		g_signal_connect(spin_rxaddr[i], "value-changed", G_CALLBACK(spin_cb), NULL);
		gtk_box_pack_start(GTK_BOX(box[1]), spin_rxaddr[i], TRUE, FALSE, 0);
		// checkbox
		check_rxaddr[i] = gtk_check_button_new();
		g_signal_connect(check_rxaddr[i], "toggled", G_CALLBACK(toggle_cb), (void*)i);
		gtk_box_pack_start(GTK_BOX(box[1]), check_rxaddr[i], TRUE, FALSE, 4);
	}
	// settings VBOX
	box[0] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_box_pack_start(GTK_BOX(box[2]), box[0], TRUE, FALSE, 0);
	// settings label
	widget = gtk_label_new("Global settings");
	gtk_box_pack_start(GTK_BOX(box[0]), widget, FALSE, FALSE, 0);
	// settings:CRC HBOX
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_set_homogeneous(GTK_BOX(box[1]), TRUE);
	gtk_box_pack_start(GTK_BOX(box[0]), box[1], FALSE, FALSE, 0);
	// settings:CRC
	widget = gtk_label_new("CRC");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	crc_spin = gtk_spin_button_new_with_range(0, 2, 1);
	g_signal_connect(crc_spin, "value-changed", G_CALLBACK(spin_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), crc_spin, TRUE, FALSE, 0);
	// settings:channel HBOX
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_set_homogeneous(GTK_BOX(box[1]), TRUE);
	gtk_box_pack_start(GTK_BOX(box[0]), box[1], FALSE, FALSE, 0);
	// settings:channel
	widget = gtk_label_new("Channel");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	channel_spin = gtk_spin_button_new_with_range(0, 127, 1);
	g_signal_connect(channel_spin, "value-changed", G_CALLBACK(spin_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), channel_spin, TRUE, FALSE, 0);
	// settings:address width HBOX
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_set_homogeneous(GTK_BOX(box[1]), TRUE);
	gtk_box_pack_start(GTK_BOX(box[0]), box[1], FALSE, FALSE, 0);
	// settings:address width
	widget = gtk_label_new("Address width");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	adw_spin = gtk_spin_button_new_with_range(2, 5, 1);
	g_signal_connect(adw_spin, "value-changed", G_CALLBACK(spin_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), adw_spin, TRUE, FALSE, 0);
	// settings:rate HBOX
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_set_homogeneous(GTK_BOX(box[1]), TRUE);
	gtk_box_pack_start(GTK_BOX(box[0]), box[1], FALSE, FALSE, 0);
	// settings:rate
	widget = gtk_label_new("Data rate");
	gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
	rate_spin = gtk_spin_button_new_with_range(0, 2, 1);
	g_signal_connect(rate_spin, "value-changed", G_CALLBACK(spin_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), rate_spin, TRUE, FALSE, 0);
	// settings: RX label
	widget = gtk_label_new("Rx Mode");
	gtk_box_pack_start(GTK_BOX(box[0]), widget, TRUE, FALSE, 0);
	// settings: RX switch
	rx_switch = gtk_switch_new();
	gtk_widget_set_sensitive(rx_switch, FALSE);
	g_signal_connect(GTK_SWITCH(rx_switch), "notify::active", G_CALLBACK(rx_sw_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[0]), rx_switch, TRUE, FALSE, 0);
	// button HBOX
	box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_box_set_homogeneous(GTK_BOX(box[1]), TRUE);
	gtk_box_pack_start(GTK_BOX(box[3]), box[1], FALSE, FALSE, 0);
	// save button
	dev_btn[1] = gtk_button_new_with_label("Save");
	gtk_widget_set_sensitive(dev_btn[1], FALSE);
	g_signal_connect(dev_btn[1], "clicked", G_CALLBACK(file_cb), (void*)1);
	gtk_box_pack_start(GTK_BOX(box[1]), dev_btn[1], TRUE, TRUE, 0);
	// load button
	dev_btn[2] = gtk_button_new_with_label("Load");
	gtk_widget_set_sensitive(dev_btn[2], FALSE);
	g_signal_connect(dev_btn[2], "clicked", G_CALLBACK(file_cb), (void*)0);
	gtk_box_pack_start(GTK_BOX(box[1]), dev_btn[2], TRUE, TRUE, 0);
	// read button
	dev_btn[3] = gtk_button_new_with_label("Read");
	gtk_widget_set_sensitive(dev_btn[3], FALSE);
	g_signal_connect(dev_btn[3], "clicked", G_CALLBACK(read_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), dev_btn[3], TRUE, TRUE, 0);
	// write button
	dev_btn[4] = gtk_button_new_with_label("Write");
	gtk_widget_set_sensitive(dev_btn[4], FALSE);
	g_signal_connect(dev_btn[4], "clicked", G_CALLBACK(write_cb), NULL);
	gtk_box_pack_start(GTK_BOX(box[1]), dev_btn[4], TRUE, TRUE, 0);

	//////////////////////////////////////
	//// Tx tab
	//////////////////////////////////////
	// hbox
//	box[0] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
//	gtk_box_pack_start(GTK_BOX(page_box[1]), box[0], TRUE, FALSE, 0);
	// vbox
	box[3] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 32);
//	gtk_box_pack_start(GTK_BOX(box[0]), box[3], TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_box[1]), box[3], TRUE, FALSE, 0);
	// fields
	for(i = 0; i < NUM_TX; i++)
	{
		// VBOX
		box[4] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_box_pack_start(GTK_BOX(box[3]), box[4], FALSE, FALSE, 0);
		// HBOX
		box[1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(box[4]), box[1], FALSE, FALSE, 0);
		// text
		widget = gtk_label_new("Address:");
		gtk_box_pack_start(GTK_BOX(box[1]), widget, TRUE, FALSE, 0);
		// address entry
		entry_txaddr[i] = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry_txaddr[i]), 10);
		gtk_box_pack_start(GTK_BOX(box[1]), entry_txaddr[i], TRUE, FALSE, 0);
		// Tx button
		tx_btn[i] = gtk_button_new_with_label("Send");
		gtk_widget_set_sensitive(tx_btn[i], FALSE);
		g_signal_connect(tx_btn[i], "clicked", G_CALLBACK(transmit_cb), (void*)i);
		gtk_box_pack_start(GTK_BOX(box[1]), tx_btn[i], TRUE, FALSE, 0);
		// data entry
		entry_txdata[i] = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry_txdata[i]), 32 * 3 - 1);
		gtk_widget_override_font(entry_txdata[i], entry_font);
		gtk_box_pack_start(GTK_BOX(box[4]), entry_txdata[i], FALSE, FALSE, 0);
	}
	//////////////////////////////////////
	//// scan tab
	//////////////////////////////////////
#ifdef USE_SCAN
	scan_pixbuf = gdk_pixbuf_new_from_inline(sizeof(graph_data), graph_data, FALSE, NULL);
	if(scan_pixbuf)
	{
		// label
		widget = gtk_label_new("Press 'Scan' to start a scan. Scanning will take few seconds to complete.");
		gtk_box_pack_start(GTK_BOX(page_box[8]), widget, FALSE, FALSE, 8);
		// button
		scan_btn = gtk_button_new_with_label("Scan");
		gtk_widget_set_sensitive(scan_btn, FALSE);
		g_signal_connect(scan_btn, "clicked", G_CALLBACK(scan_cb), NULL);
		gtk_box_pack_start(GTK_BOX(page_box[8]), scan_btn, FALSE, FALSE, 0);
		// pixbuf
		scan_widget = gtk_image_new_from_pixbuf(scan_pixbuf);
		g_object_unref(scan_pixbuf);
		gtk_box_pack_start(GTK_BOX(page_box[8]), scan_widget, FALSE, FALSE, 0);
	}
#endif
	//////////////////////////////////////
	//// start
	//////////////////////////////////////
	gtk_widget_show_all(win_main);

	gtk_main();

	if(app_icon)
		g_object_unref(app_icon);

	pango_font_description_free(entry_font);

	if(handle)
		// disconnect
		P_Deinit();

	return 0;
}

