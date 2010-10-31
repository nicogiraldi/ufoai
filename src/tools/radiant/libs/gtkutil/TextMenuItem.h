#ifndef TEXTMENUITEM_H_
#define TEXTMENUITEM_H_

#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <string>

namespace gtkutil
{

	/** Encapsulation of a GtkLabel for use in a GtkMenu.
	 */

	class TextMenuItem
	{
			// The text label
			GtkWidget* _label;

		public:

			// Constructor
			TextMenuItem (const std::string& text) :
				_label(gtk_label_new(text.c_str()))
			{
			}

			// Operator cast to GtkWidget* for packing into a menu
			operator GtkWidget* ()
			{
				GtkWidget* menuItem = gtk_menu_item_new();
				gtk_container_add(GTK_CONTAINER(menuItem), _label);
				return menuItem;
			}
	};

	// greebo: Same as above, just adds a menu item with mnemonic
	class TextMenuItemMnemonic: public TextMenuItem
	{
		public:

			// Constructor
			TextMenuItemMnemonic (const std::string& text) :
				TextMenuItem(text)
			{
			}

			// Operator cast to GtkWidget* for packing into a menu
			virtual operator GtkWidget* ()
			{
				GtkWidget* menuItem = gtk_menu_item_new_with_mnemonic(_label.c_str());
				return menuItem;
			}
	};
}

#endif /*TEXTMENUITEM_H_*/
