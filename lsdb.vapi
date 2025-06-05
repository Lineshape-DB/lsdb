/*
 * This file is part of the LSDB library & utilities.
 *
 * Copyright (C) 2025 Weizmann Institute of Science
 *
 * Author: Evgeny Stambulchik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * The license text can be found in the LGPL-3.0.txt file.
 */

[CCode (cheader_filename = "lsdb/lsdb.h", lower_case_cprefix = "lsdb_")]

namespace LSDB {
    public const int SUCCESS;
    public const int FAILURE;

    [Compact]
    [CCode (cname = "lsdb_access_t", has_type_id = false)]
    public enum Access {
        RO,
        RW,
        INIT
    }

    [Compact]
    [CCode (cname = "lsdb_units_t", has_type_id = false)]
    public enum Units {
        NONE,
        INV_CM,
        EV,
        AU,
        CUSTOM
    }

    [Compact]
    [CCode (cname = "lsdb_model_t", destroy_function = "")]
    public struct Model {
        public ulong id;
        public string name;
        public string descr;
    }

    [CCode (cname = "lsdb_model_sink_t")]
    public delegate int ModelSink(Lsdb lsdb, Model m);

    [Compact]
    [CCode (cname = "lsdb_environment_t", destroy_function = "")]
    public struct Environment {
        public ulong id;
        public string name;
        public string descr;
    }

    [CCode (cname = "lsdb_environment_sink_t")]
    public delegate int EnvironmentSink(Lsdb lsdb, Environment e);

    [Compact]
    [CCode (cname = "lsdb_radiator_t", destroy_function = "")]
    public struct Radiator {
        public ulong id;
        public string sym;
        public uint anum;
        public double mass;
        public uint zsp;
    }

    [CCode (cname = "lsdb_radiator_sink_t")]
    public delegate int RadiatorSink(Lsdb lsdb, Radiator r);

    [Compact]
    [CCode (cname = "lsdb_line_t", destroy_function = "")]
    public struct Line {
        public ulong id;
        public string name;
        public double energy;
    }

    [CCode (cname = "lsdb_line_sink_t")]
    public delegate int LineSink(Lsdb lsdb, Line l);

    [Compact]
    [CCode (cname = "lsdb_dataset_t", destroy_function = "")]
    public struct Dataset {
        public ulong id;
        public ulong mid;
        public ulong eid;
        public double n;
        public double T;
    }

    [CCode (cname = "lsdb_dataset_sink_t")]
    public delegate int DatasetSink(Lsdb lsdb, Dataset ds);

    [Compact]
    [CCode (cname = "lsdb_dataset_data_t", cprefix = "lsdb_dataset_data_")]
    public class DatasetData {
        public double n;
        public double T;
        [CCode (array_length_cname = "len", array_length_type = "size_t")]
        public double[] x;
        [CCode (array_length_cname = "len", array_length_type = "size_t")]
        public double[] y;
    }

    public void get_version_numbers(out int major, out int minor, out int nano);

    [Compact]
    [CCode (cname = "lsdb_t", cprefix = "lsdb_", free_function = "lsdb_close")]
    public class Lsdb {
        [CCode (cname = "lsdb_open")]
        public Lsdb(string fname, Access access);

        [CCode (cname = "lsdb_get_units")]
        public int get_units();

        [CCode (cname = "lsdb_get_models")]
        public int get_models(ModelSink sink);

        [CCode (cname = "lsdb_get_environments")]
        public int get_environments(EnvironmentSink sink);

        [CCode (cname = "lsdb_get_radiators")]
        public int get_radiators(RadiatorSink sink);

        [CCode (cname = "lsdb_get_lines")]
        public int get_lines(ulong rid, LineSink sink);

        [CCode (cname = "lsdb_get_datasets")]
        public int get_datasets(ulong lid, DatasetSink sink);

        [CCode (cname = "lsdb_get_dataset_data")]
        public DatasetData? get_dataset_data(int did);

        [CCode (cname = "lsdb_get_interpolation")]
        public DatasetData? get_interpolation(ulong mid, ulong eid, ulong lid,
            double n, double T, ulong len, double sigma, double gamma);

        [CCode (cname = "lsdb_get_doppler_sigma")]
        public double get_doppler_sigma(ulong lid, double T);

        [CCode (cname = "lsdb_convert_to_units", cprefix = "lsdb_")]
        public double convert_to_units(Units to_units);
    }

    [CCode (cname = "lsdb_convert_units", cprefix = "lsdb_")]
    public double convert_units(Units from_units, Units to_units);
}
