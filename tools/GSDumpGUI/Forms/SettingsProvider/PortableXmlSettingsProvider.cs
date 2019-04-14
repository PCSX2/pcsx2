using System;
using System.Collections.Specialized;
using System.Configuration;
using System.IO;
using System.Xml;
using System.Xml.Linq;

namespace GSDumpGUI.Forms.SettingsProvider
{
    public sealed class PortableXmlSettingsProvider : System.Configuration.SettingsProvider, IApplicationSettingsProvider
    {
        private const string RootNode = "configuration";
        private static string SettingsDirectory => AppDomain.CurrentDomain.BaseDirectory;
        public override string Name => nameof(PortableXmlSettingsProvider);
        private static string ApplicationSettingsFile => Path.Combine(SettingsDirectory, "portable.config");

        public static void ApplyProvider(params ApplicationSettingsBase[] settingsList)
            => ApplyProvider(new PortableXmlSettingsProvider(), settingsList);

        public override string ApplicationName
        {
            get
            {
                return nameof(GSDumpGUI);
            }
            set { }
        }

        private static XDocument GetOrCreateXmlDocument()
        {
            if (!File.Exists(ApplicationSettingsFile))
                return CreateNewDocument();
            try
            {
                return XDocument.Load(ApplicationSettingsFile);
            }
            catch
            {
                return CreateNewDocument();
            }
        }

        private static void ApplyProvider(PortableXmlSettingsProvider provider, params ApplicationSettingsBase[] settingsList)
        {
            foreach (ApplicationSettingsBase settings in settingsList)
            {
                settings.Providers.Clear();
                settings.Providers.Add(provider);
                foreach (SettingsProperty property in settings.Properties)
                    property.Provider = provider;
                settings.Reload();
            }
        }

        public override void Initialize(string name, NameValueCollection config)
        {
            if (String.IsNullOrEmpty(name))
                name = Name;
            base.Initialize(name, config);
        }

        public SettingsPropertyValue GetPreviousVersion(SettingsContext context, SettingsProperty property)
        {
            throw new NotImplementedException();
        }

        public void Reset(SettingsContext context)
        {
            if (!File.Exists(ApplicationSettingsFile))
                return;
            File.Delete(ApplicationSettingsFile);
        }

        public void Upgrade(SettingsContext context, SettingsPropertyCollection properties) { }

        private static XDocument CreateNewDocument()
        {
            return new XDocument(new XElement(RootNode));
        }

        public override SettingsPropertyValueCollection GetPropertyValues(SettingsContext context, SettingsPropertyCollection collection)
        {
            var xmlDoc = GetOrCreateXmlDocument();
            var propertyValueCollection = new SettingsPropertyValueCollection();
            foreach (SettingsProperty settingsProperty in collection)
            {
                propertyValueCollection.Add(new SettingsPropertyValue(settingsProperty)
                {
                        IsDirty = false,
                        SerializedValue = GetValue(xmlDoc, settingsProperty)
                });
            }

            return propertyValueCollection;
        }

        public override void SetPropertyValues(SettingsContext context, SettingsPropertyValueCollection collection)
        {
            var xmlDoc = GetOrCreateXmlDocument();
            foreach (SettingsPropertyValue settingsPropertyValue in collection)
                SetValue(xmlDoc, settingsPropertyValue);
            try
            {
                using (var writer = CreateWellFormattedXmlWriter(ApplicationSettingsFile))
                {
                    xmlDoc.Save(writer);
                }
            }
            catch { }
        }

        private static XmlWriter CreateWellFormattedXmlWriter(string outputFileName)
        {
            var settings = new XmlWriterSettings
            {
                    NewLineHandling = NewLineHandling.Entitize,
                    Indent = true
            };
            return XmlWriter.Create(outputFileName, settings);
        }

        private static object GetValue(XContainer xmlDoc, SettingsProperty prop)
        {
            if (xmlDoc == null)
                return prop.DefaultValue;

            var rootNode = xmlDoc.Element(RootNode);
            if (rootNode == null)
                return prop.DefaultValue;

            var settingNode = rootNode.Element(prop.Name);
            if (settingNode == null)
                return prop.DefaultValue;

            return DeserializeSettingValueFromXmlNode(settingNode, prop);
        }

        private static void SetValue(XContainer xmlDoc, SettingsPropertyValue value)
        {
            if (xmlDoc == null)
                throw new ArgumentNullException(nameof(xmlDoc));

            var rootNode = xmlDoc.Element(RootNode);
            if (rootNode == null)
                throw new ArgumentNullException(nameof(rootNode));

            var settingNode = rootNode.Element(value.Name);

            var settingValueNode = SerializeSettingValueToXmlNode(value);
            if (settingNode == null)
                rootNode.Add(new XElement(value.Name, settingValueNode));
            else
                settingNode.ReplaceAll(settingValueNode);

        }

        private static XNode SerializeSettingValueToXmlNode(SettingsPropertyValue value)
        {
            if (value.SerializedValue == null)
                return new XText("");
            switch (value.Property.SerializeAs)
            {
                case SettingsSerializeAs.String:
                    return new XText((string) value.SerializedValue);
                case SettingsSerializeAs.Xml:
                case SettingsSerializeAs.Binary:
                case SettingsSerializeAs.ProviderSpecific:
                    throw new NotImplementedException($"I don't know how to handle serialization of settings that should be serialized as {value.Property.SerializeAs}");
                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        private static object DeserializeSettingValueFromXmlNode(XNode node, SettingsProperty prop)
        {
            using (var reader = node.CreateReader())
            {
                reader.MoveToContent();
                switch (prop.SerializeAs)
                {
                    case SettingsSerializeAs.Xml:
                    case SettingsSerializeAs.Binary:
                    case SettingsSerializeAs.ProviderSpecific:
                        throw new NotImplementedException($"I don't know how to handle deserialization of settings that should be deserialized as {prop.SerializeAs}");

                    case SettingsSerializeAs.String:
                        return reader.ReadElementContentAsString();
                    default:
                        throw new ArgumentOutOfRangeException();
                }
            }
        }
    }
}
