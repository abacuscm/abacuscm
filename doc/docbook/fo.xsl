<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>
    <xsl:param name="section.autolabel" select="'1'"/>
    <xsl:param name="section.label.includes.component.label" select="'1'"/>
    <xsl:param name="funcsynopsis.style">ansi</xsl:param>
    <xsl:param name="fop1.extensions" select="'1'"/>
    <xsl:param name="toc.section.depth" select="'1'"/>
    <xsl:param name="shade.verbatim" select="'1'"/>
    <xsl:param name="paper.type" select="'A4'"/>
    <xsl:param name="generate.toc">
        /book   toc,title,figure,figure,example
    </xsl:param>
</xsl:stylesheet>
