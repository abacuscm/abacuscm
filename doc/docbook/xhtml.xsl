<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>
    <xsl:param name="section.autolabel" select="'1'"/>
    <xsl:param name="make.valid.html" select="'1'"/>
    <xsl:param name="use.id.as.filename" select="'1'"/>
    <xsl:param name="table.borders.with.css" select="'1'"/>
    <xsl:param name="section.label.includes.component.label" select="'1'"/>
    <xsl:param name="funcsynopsis.style">ansi</xsl:param>
    <xsl:param name="html.stylesheet" select="'abacuscm.css'"/>
    <xsl:param name="html.stylesheet.type" select="'text/css'"/>

    <!-- Put images in a docbook-xsl subdirectory to disguishing them -->
    <xsl:param name="img.src.path" select="'docbook-xsl/images/'"/>
    <xsl:param name="callout.graphics.path" select="'docbook-xsl/images/callouts/'"/>
    <xsl:param name="admon.graphics.path" select="'docbook-xsl/images/'"/>
    <xsl:param name="navig.graphics.path" select="'docbook-xsl/images/'"/>

    <!-- These are needed to get valid XHTML Strict output -->
    <xsl:param name="css.decoration" select="'0'"/>
    <xsl:param name="ulink.target" select="''"/>
    <xsl:param name="use.viewport" select="'0'"/>
</xsl:stylesheet>
