#! /usr/bin/env python

# libsocialweb - social data store
# Copyright (C) 2012 Thomas Bollmeier
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

from gi.repository import Gio, GLib

import dbus_util

class SocialWeb(object):

	BUS_NAME = "org.gnome.libsocialweb"
	ROOT_PATH = "/org/gnome/libsocialweb"

	def __init__(self):

		pass

	def get_services(self):

		res = []

		object = dbus_util.ProxyWrapper(
			SocialWeb.BUS_NAME,
			SocialWeb.ROOT_PATH,
			"org.gnome.libsocialweb"
			)

		data = object.GetServices()
		services = data.get_child_value(0)
		num_services = services.n_children()

		for i in xrange(0, num_services):
			service = services.get_child_value(i)
			res.append(service.get_string())

		return res

	def get_service(self, name):

		service_names = self.get_services()

		if name in service_names:
			return dbus_util.ProxyWrapper(
				SocialWeb.BUS_NAME,
				SocialWeb.ROOT_PATH + "/Service/" + name,
				"org.gnome.libsocialweb.Service"
				)
		else:
			return None

class FlickrClient(object):

	def __init__(self, flickr_object):

		self.object = flickr_object

	def get_static_caps(self):

		res = []

		caps = self.object.GetStaticCapabilities().get_child_value(0)
		for i in xrange(0, caps.n_children()):
			cap = caps.get_child_value(i)
			res.append(cap.get_string())

		return res

	def get_dynamic_caps(self):

		res = []

		caps = self.object.GetDynamicCapabilities().get_child_value(0)
		for i in xrange(0, caps.n_children()):
			cap = caps.get_child_value(i)
			res.append(cap.get_string())

		return res

	def open_own_items_view(self):

		return self._open_view("own")

	def open_search_results_view(self, search_text):

		return self._open_view(
			"x-flickr-search", 
			text = search_text
			)

	def _open_view(self, query_name, **parameters):

		obj = self.object.get_interface("org.gnome.libsocialweb.Query")
		data = obj.OpenView("(sa{ss})", query_name, parameters)
		if data:
			view_path = data.get_child_value(0).get_string()
			view_object = dbus_util.ProxyWrapper(
				self.object.bus_name,
				view_path,
				"org.gnome.libsocialweb.ItemView"
				)
			return view_object
		else:
			return None

def on_items_added(object, signal_name, parameters, user_data):

	print "Items added:"
	
	items = parameters.get_child_value(0)
	
	for i in xrange(0, items.n_children()):
	
		item = items.get_child_value(i)
	
		service = item.get_child_value(0)
		id = item.get_child_value(1)
		properties = item.get_child_value(3)
		photo_url = properties.lookup_value("x-flickr-photo-url", None)
		
		print "\tService: " + service.get_string()
		print "\tId: " + id.get_string()
		if photo_url:
			print "\tPhoto-URL: " + photo_url.get_string()
	
	#main_loop.quit()

def on_caps_changed(object, signal_name, parameters, user_data):

	caps = parameters.get_child_value(0)

	for i in xrange(0, caps.n_children()):
		if i == 0:
			print "Changed capabilities:"		
		print "\t%s" % caps.get_child_value(i).get_string()

def cmdline_args_to_string(args):

	res = ""
	for arg in args:
		if res:
			res += " "
		res += arg

	return res

##### main #####

import sys

main_loop = GLib.MainLoop()

service = SocialWeb().get_service("flickr")
if not service:
	print "Flickr service not found"
	exit(1)

flickr = FlickrClient(service)
flickr.object.connect("CapabilitiesChanged", on_caps_changed)
flickr.object.CredentialsUpdated()

args = sys.argv[1:]

if args:
	item_view = flickr.open_search_results_view(cmdline_args_to_string(args))	
else:
	item_view = flickr.open_own_items_view()

if not item_view:
	print "ItemView cannot be opened!"
	exit(1)

print "ItemView: %s" % item_view.object_path

item_view.connect("ItemsAdded", on_items_added)
item_view.Start()

main_loop.run()
