#!/usr/bin/python
from gi.repository import Gio, GLib

class SocialWeb(object):

	BUS_NAME = "org.gnome.libsocialweb"
	ROOT_PATH = "/org/gnome/libsocialweb"

	def __init__(self):

		self._conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)

	def get_services(self):

		res = []

		object = self._create_object(
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
			
			return self._create_object(
				SocialWeb.BUS_NAME,
				SocialWeb.ROOT_PATH + "/Service/" + name,
				"org.gnome.libsocialweb.Service"
				)

		else:
			return None

	def _create_object(self, bus_name, object_path, interface_name):

		proxy = Gio.DBusProxy.new_sync(
			self._conn,
			Gio.DBusProxyFlags.NONE,
			None,
			bus_name,
			object_path, 
			interface_name, 
			None
			)

		return DBusObject(proxy)

class DBusObject(object):

	def __init__(self, proxy):

		self._proxy = proxy

	def call_method(self, name, *args):

		return self._proxy.call_sync(
			name,
			None, # <-- TODO: pass parameters from args
			Gio.DBusCallFlags.NONE,
			-1, # <-- no timeout
			None # <-- cancellable object
			)

	def connect(self, signal, handler, user_data=None):

		self._proxy.connect("g-signal", handler, user_data)

##### main #####

def on_caps_changed(*args):

	print args

sw = SocialWeb()

flickr = sw.get_service("flickr")
if not flickr:
	print "Flickr service not found"
	exit(1)

flickr.connect("CapabilitiesChanged", on_caps_changed)

caps = flickr.call_method("GetStaticCapabilities").get_child_value(0)

print "Static Capabilities:\n"

for i in xrange(0, caps.n_children()):
	cap = caps.get_child_value(i)
	print cap.get_string()

caps = flickr.call_method("GetDynamicCapabilities").get_child_value(0)

print "\nDynamic Capabilities:\n"

for i in xrange(0, caps.n_children()):
	cap = caps.get_child_value(i)
	print cap.get_string()

#GLib.MainLoop().run()
