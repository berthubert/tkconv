<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0"
>
  <xsl:output method="html"
    version="5.0"
    doctype-system="about:legacy-compat"
    encoding="UTF-8"
    indent="yes"
  />

  <!-- This XSLT converts Dutch Official Publication documents into HTML. The input vocabulary is
  documented in
  http://technische-documentatie.oep.overheid.nl/repository/schemas/op-consolidated/op-consolidated_2014-05-15/xsd/op-xsd-2014-05-15.xsd -->

  <xsl:template match="/*">
    <html>
      <head>
        <title>
          <xsl:apply-templates select="*" mode="title" />
        </title>
        <link
          rel="stylesheet"
          href="../pico.min.css"
        />
        <style>
        </style>
      </head>
      <body>
        <main class="container">
          <xsl:apply-templates select="*" />
        </main>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="metadata|kamerstukkop" />

  <xsl:template match="kamerstuk">
    <article>
      <xsl:apply-templates select="*" />
    </article>
  </xsl:template>

  <xsl:template match="dossier">
    <h1 class="dossiertitel">
      <xsl:apply-templates select="*" />
    </h1>
  </xsl:template>

  <xsl:template match="dossiernummer">
    <span class="dossiernummer">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <xsl:template match="dossiernr|dossier/titel">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="stuk">
    <div class="stuk">
      <h1 class="stuktitel">
        <xsl:apply-templates select="stuknr|titel" />
      </h1>
      <xsl:apply-templates select="datumtekst" />
      <xsl:apply-templates select="officiele-inhoudsopgave|voorstel-wet|amendement|algemeen" />
    </div>
    <xsl:if
      test=".//noot">
      <div id="noten">
        <hr />
        <xsl:apply-templates select=".//noot" mode="noten" />
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template match="stuknr">
    <span class="stuknummer">
      <xsl:apply-templates />
    </span>
    <xsl:text> </xsl:text>
  </xsl:template>

  <xsl:template match="ondernummer|stuk/titel">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="noot">
    <xsl:apply-templates select="noot.nr" />
  </xsl:template>

  <xsl:template match="noot.nr">
    <sup>
      <a class="nootnum supernote-click-ID-{generate-id()}" href="#ID-{generate-id()}">
        <xsl:apply-templates />
      </a>
    </sup>
  </xsl:template>

  <xsl:template match="noot" mode="noten">
    <div class="voet noot snp-mouseoffset snb-pinned notedefault"
      id="supernote-note-ID-{generate-id()}">
      <h5 class="note-close"><a class="note-close" href="#close-ID-{generate-id()}">X</a> Noot </h5>
      <xsl:apply-templates mode="noten" />
    </div>
  </xsl:template>

  <xsl:template match="noot.nr" mode="noten">
    <sup>
      <span class="nootnum" id="ID-{generate-id()}">
        <xsl:apply-templates />
      </span>
    </sup>
  </xsl:template>

  <xsl:template match="noot.al" mode="noten">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="datumtekst">
    <p class="kamerstukdatum">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="datum">
    <span class="datum">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <xsl:template match="amendement">
    <div class="amendement">
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template match="amendement-lid">
    <div class="amendement-lid">
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template match="lidnr">
    <p class="lid">
      <span class="lidnr">
        <xsl:apply-templates />
      </span>
    </p>
  </xsl:template>

  <xsl:template match="wat">
    <p class="wat">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="al">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="divisie|kop|tekst-sluiting">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="divisie/kop/titel">
    <h2 class="divisiekop1">
      <xsl:apply-templates />
    </h2>
  </xsl:template>

  <xsl:template match="al-groep">
    <div class="alineagroep">
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template match="lijst[@type='expliciet']">
    <ul class="expliciet whitespace-small">
      <xsl:apply-templates />
    </ul>
  </xsl:template>

  <xsl:template match="li">
    <li class="li">
      <xsl:apply-templates select="*[not(self::li.nr)]" />
    </li>
  </xsl:template>

  <xsl:template match="li[li.nr]/al[1]">
    <p class="labeled">
      <xsl:apply-templates select="preceding-sibling::li.nr" />
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="li.nr">
    <span class="ol">
      <xsl:apply-templates />
      <xsl:text> </xsl:text>
    </span>
  </xsl:template>

  <xsl:template match="ondertekening">
    <p class="ondertekening">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="naam">
    <span class="naam">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <xsl:template match="achternaam">
    <span class="achternaam">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <!-- Helps the author of this xslt to see what still needs to be done -->
  <!-- <xsl:template match="*">
    <xsl:message terminate="no">
      <xsl:text>Unhandled element </xsl:text>
      <xsl:value-of select="local-name()" />
    </xsl:message>
    <xsl:apply-templates />
  </xsl:template> -->

  <xsl:template match="*" mode="title"> TODO </xsl:template>
</xsl:stylesheet>