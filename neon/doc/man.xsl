<?xml version='1.0'?>

<!-- This file wraps around the xmlto db2man XSL stylesheet to
customise some parameters. -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<xsl:import href="/usr/share/xmlto/xsl/db2man/docbook.xsl"/>

<!-- use an improved xref -->
<xsl:import href="./xref-man.xsl"/>

</xsl:stylesheet>
