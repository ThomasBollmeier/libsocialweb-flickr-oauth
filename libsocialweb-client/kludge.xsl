<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:gi="http://www.gtk.org/introspection/core/1.0"
                xmlns:glib="http://www.gtk.org/introspection/glib/1.0"
                xmlns:c="http://www.gtk.org/introspection/c/1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		version="1.0">

  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="glib:signal[@name='items-added' or
    @name='items-changed' or @name='items-removed']">
    <glib:signal name="{@name}">
      <return-value transfer-ownership="none">
        <type name="none"/>
      </return-value>
      <parameters>
        <parameter name="items" transfer-ownership="none">
          <type name="GLib.List" c:type="GList*">
            <type name="SocialWebClient.Item" c:type="SwItem*"/>
          </type>
        </parameter>
      </parameters>
    </glib:signal>
  </xsl:template>

  <xsl:template match="glib:signal[@name='contacts-added' or
    @name='contacts-changed' or @name='contacts-removed']">
    <glib:signal name="{@name}">
      <return-value transfer-ownership="none">
        <type name="none"/>
      </return-value>
      <parameters>
        <parameter name="contacts" transfer-ownership="none">
          <type name="GLib.List" c:type="GList*">
            <type name="SocialWebClient.Contact" c:type="SwContact*"/>
          </type>
        </parameter>
      </parameters>
    </glib:signal>
  </xsl:template>

  <xsl:template match="gi:record[@name='Item']/gi:field[@name='props']">
    <field name="props" writable="1">
      <type name="GLib.HashTable" c:type="GHashTable*">
        <type name="utf8"/>
        <type name="utf8"/>
      </type>
    </field>
  </xsl:template>

  <xsl:template match="gi:record[@name='Contact']/gi:field[@name='props']">
    <field name="props" writable="1">
      <type name="GLib.HashTable" c:type="GHashTable*">
        <type name="utf8"/>
        <type name="utf8"/>
      </type>
    </field>
  </xsl:template>

</xsl:stylesheet>
