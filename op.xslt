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
          href="html/pico.min.css"
        />
        <link
          rel="stylesheet"
          href="html/op.css"
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

  <xsl:template match="dossiernr|dossier/titel|begrotingshoofdstuk">
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

  <xsl:template match="ondernummer|herdruk|stuk/titel|vrije-tekst|tekst|wijzig-artikel">
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

  <xsl:template
    match="amendement|amendement-lid|algemeen|voorstel-wet|aanhef|wettekst|
    wijzig-lid|wijziging|artikel|voorstel-sluiting|slotformulering">
    <div class="{local-name()}">
      <xsl:apply-templates select="*" />
    </div>
  </xsl:template>

  <xsl:template
    match="artikeltekst">
    <div class="artikeltekst">
      <xsl:choose>
        <xsl:when test="lid">
          <ul class="artikel_leden whitespace-small">
            <xsl:apply-templates select="*" />
          </ul>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="*" />
        </xsl:otherwise>
      </xsl:choose>
    </div>
  </xsl:template>

  <xsl:template match="lid">
    <li>
      <xsl:apply-templates select="*" />
    </li>
  </xsl:template>

  <xsl:template match="lidnr">
    <p class="lid">
      <span class="lidnr">
        <xsl:apply-templates />
      </span>
    </p>
  </xsl:template>

  <xsl:template match="definitielijst[@plaatsing='inline']">
    <dl class="inline">
      <xsl:apply-templates select="*" />
    </dl>
  </xsl:template>

  <xsl:template match="definitie-item">
    <dl>
      <xsl:apply-templates select="*" />
    </dl>
  </xsl:template>

  <xsl:template match="definitie-item/li.nr" />

  <xsl:template match="term[preceding-sibling::li.nr]">
    <dt>
      <span class="ol">
        <xsl:apply-templates select="preceding-sibling::li.nr/node()" />
      </span>
      <xsl:text> </xsl:text>
      <xsl:apply-templates />
    </dt>
  </xsl:template>

  <xsl:template match="term">
    <dt>
      <xsl:apply-templates />
    </dt>
  </xsl:template>

  <xsl:template match="definitie">
    <dd>
      <xsl:apply-templates select="*" />
    </dd>
  </xsl:template>

  <xsl:template match="wat|wij|considerans.al">
    <p class="{local-name()}">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="wijziging/nr" />

  <xsl:template match="wijziging/wat[preceding-sibling::nr]">
    <p class="wat labeled">
      <span class="nr">
        <xsl:apply-templates select="preceding-sibling::nr/node()" />
      </span>
      <xsl:text> </xsl:text>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="al">
    <p>
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="afkondiging/al">
    <p class="afkondiging">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <xsl:template match="nadruk[@type='vet']">
    <strong class="vet">
      <xsl:apply-templates />
    </strong>
  </xsl:template>

  <xsl:template match="extref[@doc]">
    <a href="/tkconv/op/{@doc}">
      <xsl:apply-templates />
    </a>
  </xsl:template>

  <xsl:template match="dossierref">
    <a href="/tkconv/ksd.html?ksd={@dossier}">
      <xsl:apply-templates />
    </a>
  </xsl:template>

  <xsl:template match="divisie|kop|tekst-sluiting|considerans|afkondiging">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="divisie/kop">
    <h2 class="divisiekop1">
      <xsl:apply-templates select="*" />
    </h2>
  </xsl:template>

  <xsl:template match="wijzig-artikel/kop">
    <h2 class="wijzig-artikel_kop">
      <xsl:apply-templates select="*" />
    </h2>
  </xsl:template>

  <xsl:template match="artikel/kop">
    <h3 class="artikel_kop no-toc">
      <xsl:apply-templates select="*" />
    </h3>
  </xsl:template>

  <xsl:template match="kop/*">
    <xsl:apply-templates />
    <xsl:if test="position() != last()">
      <xsl:text> </xsl:text>
    </xsl:if>
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

  <xsl:template match="naam|achternaam|functie|dossiernummer|datum">
    <span class="{local-name()}">
      <xsl:apply-templates />
    </span>
  </xsl:template>

  <xsl:template match="table">
    <table>
      <xsl:apply-templates />
    </table>
  </xsl:template>

  <xsl:template match="tgroup">
    <colgroup>
      <xsl:apply-templates select="colspec" />
    </colgroup>
    <xsl:apply-templates
      select="thead|tbody" />
  </xsl:template>

  <xsl:template match="colspec">
    <col />
  </xsl:template>

  <xsl:template match="thead">
    <thead>
      <xsl:apply-templates />
    </thead>
  </xsl:template>

  <xsl:template match="tbody">
    <tbody>
      <xsl:apply-templates />
    </tbody>
  </xsl:template>

  <xsl:template match="row">
    <tr>
      <xsl:apply-templates />
    </tr>
  </xsl:template>

  <xsl:template match="entry">
    <td>
      <xsl:apply-templates />
    </td>
  </xsl:template>

  <xsl:template match="sup">
    <sup>
      <xsl:apply-templates />
    </sup>
  </xsl:template>

  <xsl:template match="inf">
    <sub>
      <xsl:apply-templates />
    </sub>
  </xsl:template>

  <xsl:template match="voorstel-sluiting/label">
    <p class="gegeven">
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <!-- Helps the author of this xslt to see what still needs to be done -->
  <xsl:template match="*">
    <xsl:message terminate="no">
      <xsl:text>Unhandled element </xsl:text>
      <xsl:value-of select="local-name()" />
    </xsl:message>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="*" mode="title"> TODO </xsl:template>
</xsl:stylesheet>