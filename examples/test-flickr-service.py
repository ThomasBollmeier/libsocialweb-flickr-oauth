#!/usr/bin/python
from gi.repository import Gio, GLib

class SocialWeb(object):

	BUS_NAME = "org.gnome.libsocialweb"
	ROOT_PATH = "/org/gnome/libsocialweb"

	def __init__(self):

		self._conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)

	def get_services(self):

		res = []

		object = self.create_object(
			SocialWeb.BUS_NAME,
			SocialWeb.ROOT_PATH,
			"org.gnome.libsocialweb"
			)

		data = object.call_method("GetServices")
		services = data.get_child_value(0)
		num_services = services.n_children()

		for i in xrange(0, num_services):
			service = services.get_child_value(i)
			res.append(service.get_string())

		return res

	def get_service(self, name):

		service_names = self.get_services()

		if name in service_names:
			return self.create_object(
				SocialWeb.BUS_NAME,
				SocialWeb.ROOT_PATH + "/Service/" + name,
				"org.gnome.libsocialweb.Service"
				)
		else:
			return None

	def create_object(self, bus_name, object_path, interface_name):

		return DBusObject(self._conn, bus_name, object_path, interface_name)

class DBusObject(object):

	def __init__(self, conn, bus_name, object_path, interface_name):

		self.bus_name = bus_name
		self.object_path = object_path
		self.interface_name = interface_name

		self._proxy = Gio.DBusProxy.new_sync(
			conn,
			Gio.DBusProxyFlags.NONE,
			None,
			bus_name,
			object_path, 
			interface_name, 
			None
			)

	def call_method(self, name, args = None):

		return self._proxy.call_sync(
			name,
			args,
			Gio.DBusCallFlags.NONE,
			-1, # <-- no timeout
			None # <-- cancellable object
			)

	def connect(self, signal, handler, user_data=None):

		self._proxy.connect("g-signal", handler, user_data)

class FlickrClient(object):

	def __init__(self, socialweb, flickr_object):

		self._socialweb = socialweb
		self._flickr = flickr_object

	def get_static_caps(self):

		res = []

		caps = self._flickr.call_method("GetStaticCapabilities").get_child_value(0)
		for i in xrange(0, caps.n_children()):
			cap = caps.get_child_value(i)
			res.append(cap.get_string())

		return res

	def get_dynamic_caps(self):

		res = []

		caps = self._flickr.call_method("GetDynamicCapabilities").get_child_value(0)
		for i in xrange(0, caps.n_children()):
			cap = caps.get_child_value(i)
			res.append(cap.get_string())

		return res

	def open_own_items_view(self):

		query = self._socialweb.create_object(
			self._flickr.bus_name,
			self._flickr.object_path,
			"org.gnome.libsocialweb.Query"
			)

		args = GLib.Variant("(sa{ss})", (
			"own",
			{}
			))

		data = query.call_method("OpenView", args)
		if data:
			view_path = data.get_child_value(0).get_string()
			view_object = self._socialweb.create_object(
				self._flickr.bus_name,
				view_path,
				"org.gnome.libsocialweb.ItemView"
				)
			return ItemView(view_object)
		else:
			return None

class ItemView(object):

	def __init__(self, view_object):

		self._view_object = view_object

	def close(self):

		self._view_object.call_method("Close")

	def _get_object_path(self):

		return self._view_object.object_path

	object_path = property(_get_object_path)

##### main #####

sw = SocialWeb()

service = sw.get_service("flickr")
if not service:
	print "Flickr service not found"
	exit(1)

flickr = FlickrClient(sw, service)

print "Static capabilities:"
for cap in flickr.get_static_caps():
	print "\t" + cap
print

print "Dynamic capabilities:"
for cap in flickr.get_dynamic_caps():
	print "\t" + cap

item_view = flickr.open_own_items_view()

if item_view:
	print "ItemView: %s" % item_view.object_path
	item_view.close()

#GLib.MainLoop().run()
