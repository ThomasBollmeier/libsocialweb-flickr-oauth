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

class Connection:

	SESSION = Gio.bus_get_sync(Gio.BusType.SESSION, None)

class ProxyWrapper(object):
	"""
	Wrapper class for DBusProxy instances
	"""

	def __init__(self, 
		bus_name, 
		object_path, 
		interface_name,
		connection = None
		):

		self.bus_name = bus_name
		self.object_path = object_path
		self.interface_name = interface_name

		self._proxy = Gio.DBusProxy.new_sync(
			connection or Connection.SESSION,
			Gio.DBusProxyFlags.NONE,
			None,
			bus_name,
			object_path, 
			interface_name, 
			None
			)

		self._proxy.connect("g-signal", self._on_g_signal, None)
		self._handlers = {}

	def get_interface(self, interface_name):

		return ProxyWrapper(self.bus_name, self.object_path, interface_name)

	def call_method(self, name, format_str="", *args):

		if format_str:
			parameters = GLib.Variant(format_str, tuple(args))
		else:
			parameters = None

		return self._proxy.call_sync(
			name,
			parameters,
			Gio.DBusCallFlags.NONE,
			-1, # <-- no timeout
			None # <-- cancellable object
			)

	def connect(self, signal_name, handler, user_data=None):

		if not signal_name in self._handlers:
			self._handlers[signal_name] = [(handler, user_data)]
		else:
			self._handlers[signal_name].append((handler, user_data))

	def _on_g_signal(self, 
		proxy, 
		sender_name, 
		signal_name,
		parameters,
		user_data
		):

		if signal_name in self._handlers:
			for handler, data in self._handlers[signal_name]:
				handler(self, signal_name, parameters, data)

	def __getattr__(self, attr_name):

		# Interpret attribute name as the name of a method:
		return _MethodCall(self, attr_name)

class _MethodCall(object):

	def __init__(self, dbus_object, method_name):

		self._dbus_obj = dbus_object
		self._name = method_name

	def __call__(self, format_str="", *args):

		return self._dbus_obj.call_method(
			self._name, 
			format_str,
			*args
			)
