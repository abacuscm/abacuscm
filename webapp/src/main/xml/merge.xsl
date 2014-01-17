<?xml version="1.0" encoding="UTF-8"?>
<!--
  Copyright (C) 2010-2011  Bruce Merry and Carl Hultquist

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
-->
<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns="http://www.w3.org/1999/xhtml"
    xmlns:h="http://www.w3.org/1999/xhtml"
    version="1.0">

    <xsl:output method="xml"
        doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
        doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"/>

    <!-- Clean up some bad DocBook output (not strict XHTML) -->
    <xsl:template match="h:ol/@type" />
    <xsl:template match="h:br/@clear" />

    <!-- Identity transform -->
    <xsl:template match="node()|@*">
        <xsl:copy>
            <xsl:apply-templates select="node()|@*"/>
        </xsl:copy>
    </xsl:template>

    <xsl:template match="id('body-template')">
        <xsl:apply-templates select="document('body.html')//h:body/*"/>
    </xsl:template>

    <xsl:template match="h:object">
        <xsl:apply-templates select="document('../../../../doc/html/contestant.html')//h:body/*"/>
    </xsl:template>

    <!-- Fill in standings page -->
    <xsl:template match="id('standings-template')">
        <xsl:apply-templates select="document('body.html')//h:div[@id = 'tab-standings']/*"/>
    </xsl:template>
</xsl:stylesheet>
